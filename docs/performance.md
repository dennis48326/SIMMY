# Performance: frame hitches

On a PC the game needs a small fraction of a frame (well under 1 ms of game logic per frame on a
desktop CPU), but players saw "random slowdowns": single frames of 50-500 ms in an otherwise smooth
60 Hz game. This page is what was measured, what caused them, what was changed, the switches for each
change, and how to capture a profile on your own PC.

## The frame-hitch profiler (`perf_log.txt`)

Off by default. Turn it on with any of:

- `F3` while playing (on / off at any time),
- the F1 menu: *Graphics* page, "Frame-hitch profiler" (it also shows the median frame time, the worst
  frame of the last 5 s and the hitch count while it runs),
- `perf = on` in a `[debug]` section of `simmy.ini`,
- `SIMMY_PERF=1` (tests; `SIMMY_PERF_LOG=<file>` sets the file, `SIMMY_PERF_MS=<ms>` the threshold).

It writes `perf_log.txt` next to `simmy.exe` (the same folder as `game_log.txt`). Code:
[`src/platform/pc_perf.cpp`](../src/platform/pc_perf.cpp), hooks marked `SIMMY_PERF_SCOPE` /
`perf::count_*`. It only reads clocks and counters: the game, the simulation and network play see
nothing of it (the determinism hash is the same with it on).

**What it measures.** A frame is one `EApp::SystemUpdate`, from the end of one frame-limiter sleep to
the end of the next. Its main-thread time is split into phases, each exclusive (a load inside a sim
tick counts as load, not as sim):

| Phase | What |
|---|---|
| `game` | everything in the game's update not listed below (UI, camera, animation, objects) |
| `sim` | simulation ticks (`cSimulatorImpl::SimulateOneTick`); `(n ticks)` in the frame |
| `route` | path finding (`XRoute::FindPath`, `ConstructGoals`) |
| `draw` | the game's draw recording (`ESimsApp::UpdateDraw`): display lists, GX commands |
| `gpu` | aurora's end of frame: GX command processing, texture conversion, encode, submit, present |
| `acquire` | aurora's begin of frame: waiting for a swapchain image (vsync / GPU back-pressure) |
| `events` | the SDL window and input event pump |
| `audio` | the audio updates on the main thread |
| `load` | resource loads (`EResourceLoaderImpl::DoLoadResource`) without their disc reads |
| `disc` | reads from the disc image or data folder (and mod files) that were not in RAM |
| `card` | memory card I/O |
| `net` | network play: waiting for the other PC |
| `pace` | the frame limiter's sleep, as long as it was asked to be |
| `late` | the frame limiter's sleep beyond its deadline (timer resolution, OS scheduler) |

Per frame it also counts: allocations (`EAHeap`), disc reads (main thread and other threads, per
file), resource loads (per manager; a single load over 1 ms is named with its id), resource cache
evictions (`FreeResourceManager`), GPU pipelines built and queued (aurora), textures converted and
uploaded and texture cache evictions (aurora, `cmake/aurora_patches/0004-texture-stat-totals.patch`),
and the growth of the game's own log files (`game_log.txt` / `game_out.txt`).

**Hitches.** A frame is a hitch when it took more than 25 ms *and* more than 1.5x the median of the
last 300 frames. Each hitch is written at once:

```
[hitch] frame 2325 (in a house): 479.1 ms (median 16.7) cause disc
  phases: game 0.5 draw 1.0 disc 477.5
  counts: allocs 0 (0.0 KB), disc reads 1 (64.0 KB) + 0 bg (0.0 KB), loads 1 + 0 bg, evictions 0, pipelines +0 (queued 0), textures +0 (0.0 KB, 0 evicted), log 0 B
  loads: samples x1 24.0 KB 477.51 ms
  disc: DATA/SAMPLES.ARC x1 64.0 KB 477.49 ms
  load samples 1F2CCA1D 24.0 KB 477.51 ms
```

The *cause* is the phase most over its usual (running average) time, with `(pipelines)` /
`(texture uploads)` added to `gpu` when the frame built pipelines or converted textures. Frames are
filed under a context: `menus` (no house), `loading` (a loading screen is up) and `in a house` (live,
buy and build mode); hitches behind a loading screen are expected and matter little. Once a minute a
`[minute]` line gives that minute's median, p99, max and hitches. At exit (any clean exit: F1 Quit,
closing the window) a summary follows: per context the frame-time distribution (median, p95, p99,
p99.9, max, frames over 25/34/50/100 ms), the mean of each phase, the totals of everything counted,
`mean |frame - 16.67 ms|` (how steady the pacing is), hitches by cause, which hitch frames also had
loads / disc reads / pipelines / texture uploads / ..., the resource loads made while in a house, and
the 10 worst frames in full. With an F3 toggle the header gets `[perf] on` / `off` lines; the first
line after the header (window runs) is the setup: backend, display refresh rate, vsync, fps cap, MSAA,
frame pacing and disc cache modes, RAM, CPUs.

## What was found

Measured in the Linux container (2 cores, no GPU: software rendering), headless runs for the CPU and
disc side (`SIMMY_SOAK` random play on lot 9: walking, the camera, interactions, buy and build mode,
pause menus, quitting to the menu; with and without the 961 converted PC mods) and offscreen runs for
the GPU side (absolute GPU times are not representative there, only what happens in which frame).
To get the disc's real cost the OS file cache was dropped before each run (a first session after
booting the PC; or an image on a hard disk).

1. **Synchronous disc reads during play: the main cause.** The GameCube read resources from the disc
   as the game needed them and kept only what fit its 24 MB; the port did the same through
   `data_path.cpp`. Every first use in a session of a sound (`samples.arc`, by far the most), an
   animation (a sim starting a new interaction), a texture, a shader, a Flash screen or a dataset was
   a read of 4-200 KB at a random place in the 1.4 GB image, on the main thread, inside the frame.
   With a cold file cache each read cost 10-500 ms. In a 5,160-frame in-house session (86 s) that was
   59 sample loads (5.2 s in all), 31 animation loads (1.6 s), 14 texture and 14 shader loads (1.3 s
   each), 4 Flash loads and a dataset: **47 hitch frames, 43 of them disc, 8.0 s of stall** (p99.9
   266 ms, worst 870 ms). The OS file cache hides it from the second session on as long as the image
   stays cached, which is why it looked random. Behind loading screens the same reads made house loads
   slower (one 2.3 s frame of 635 loads).
2. **Frame limiter precision.** The limiter slept with `std::this_thread::sleep_until`, which on
   Windows is `Sleep()` in whole milliseconds rounded up (1 ms timer resolution, which SDL asks for:
   wakes 1-2 ms late, more under load). The game turns every frame's duration into 60 Hz retraces
   (`EEngine::RetraceUpdate`: 25 ms or more counts as two, and the game clock jumps 33 ms), so late
   wake-ups near a deadline become visible stutter, and with `vsync = on` on a 60 Hz display the
   limiter's clock and the display's beat against each other. Linux measurement: mean oversleep
   0.15-0.19 ms a frame with `sleep_until`, 0.02-0.04 ms with the new precise sleep (Windows'
   difference is larger: confirm with `perf_log.txt`'s `late` phase).
3. **GPU pipeline builds.** aurora builds a GPU pipeline the first time a material / TEV setup is
   drawn, on its pipeline thread; the draw is skipped until it is ready (in a window), so the cost is
   a missing object for a frame or two plus whatever the build holds up on the main thread (in the
   offscreen runs, 22 in-house frames with `gpu (pipelines)`, 25-120 ms on the software renderer).
   aurora keeps the pipelines it built in `pipeline_cache.db` and builds them again in the background
   at the next start, so this is a first-session cost. The port keeps that cache per executable
   (`cache_<size>_<time>` under `%APPDATA%\SIMMY`, see `pc_main.cpp`: a cache from another build held
   stale pipelines), so **every rebuild of `simmy.exe` starts cold**, also the D3D12 shader cache
   (`dawn_cache.db`) next to it. Not changed (see below).
4. **Texture conversion.** GameCube textures are converted (palettes, I4/I8/IA, RGB5A3; CMPR stays BC1)
   and uploaded the first time they are drawn, on aurora's GX thread inside the frame (`gpu`): a few
   ms for a new lot's worth, nothing later. aurora's 128 MB content cache never evicted in any session
   (0 evictions), so this is not a repeating cost.
5. **Not causes** (measured, nothing to fix): the resource caches never evicted in these sessions
   (0 `FreeResourceManager` evictions with the GameCube's 800-entry free lists: there is no 24 MB
   limit on PC, `EAHeap` is the host allocator and `Compact()` does nothing); allocations are a few
   hundred per minute; sim ticks and path finding are well under 1 ms; memory card I/O only happens in
   the save screens; the game's log output during play is a few hundred bytes a minute (0 bytes in
   almost every frame); the event pump and the audio update are below 0.1 ms.

## What was changed

All of it is I/O, caching and timing outside the simulation: the game gets the same bytes and the same
input in the same order. The determinism hash (`SIMMY_DET` replay of `L.rec`, 24,000 ticks, plain and
with `SIMMY_DET_JITTER=10`) is byte-identical, network play stays in lockstep.

| Change | Switch (default) | Where |
|---|---|---|
| **Disc cache.** At start a background thread (low CPU and I/O priority) reads the archives the game loads resources from, `index.ind` .. `datasets.arc` (22 files, 825 MB; not `movies.arc` and `audiostr.arc`, which are streamed by their own threads), start to end in 4 MiB chunks into RAM. A read whose chunks are in is a `memcpy`; the rest still go to the disc, so nothing waits for the cache. From a cold disc it filled in 19.5 s in the container (0.6 s from the file cache), during the intro. | `disc_cache = auto` (`on`/`ram`, `warm`, `off`; `SIMMY_DISC_CACHE`): `auto` = RAM with 8 GB or more, else `warm` (the same reads through the OS file cache, nothing kept) | `src/platform/data_path.cpp` |
| Mod files read once stay in RAM (up to 512 MB). | with `disc_cache` (off when it is `off`) | `src/platform/pc_mods.cpp` |
| **Precise frame pacing.** The limiter sleeps with `SDL_DelayPrecise` (OS sleep, then a short spin for the last moment) to each 60 Hz deadline. | `frame_pacing = precise` (`sleep` = before; `SIMMY_FRAME_PACING`) | `src/platform/pc_main.cpp` |
| **Vsync pacing (opt-in).** With vsync on and a 59-61 Hz display the display paces the game: the limiter only stops frames faster than 15 ms, each frame waits for its refresh in aurora's present. Not used in network play, falls back to `precise` when the display is not 59-61 Hz or when frames keep coming faster than 60 Hz (vsync not honoured). | `frame_pacing = vsync` | `src/platform/pc_main.cpp` |
| The profiler itself. | `[debug] perf = off`, F3 | `src/platform/pc_perf.cpp` |

### Before / after

Same scripted sessions, cold file cache, headless (in a house = live, buy and build mode):

| Session | | in-house frames | hitches | worst | p99.9 | stall in hitches | RSS |
|---|---|---|---|---|---|---|---|
| lot 9, soak, 7000 frames | `disc_cache=off` | 5164 | 47 (43 disc) | 869.7 ms | 266 ms | 8.0 s | 80 MB |
| | `disc_cache=on` | 5146 | 0 | 24.4 ms | 20.8 ms | 0 | 925 MB |
| lot 9 + 961 mods, soak, 9000 frames | `disc_cache=off` | 7158 | 2 | 27.6 ms | 20.3 ms | 19 ms | 87 MB |
| | `disc_cache=on` | 7157 | 0 | 23.9 ms | 19.3 ms | 0 | 933 MB |

Offscreen (software GPU, lot 9 + mods, soak, 5000 frames): RSS 1.09 GB without the cache, 1.93 GB with
it; per in-house session about 100-110 GPU pipelines built and ~215 textures converted (5 MB) with a
fresh pipeline cache, 0 texture cache evictions, 0 resource cache evictions; three in-house
screenshots of a `SIMMY_DET` replay are pixel-identical to the previous build's.

(The mods session's "off" run happened to find most of its data still in the host's cache: 215 reads
took 110 ms in all. The cost of a disc read depends on the disc and the cache, which is the point.)

## Per-frame GPU buffers (build dv)

aurora records each frame into fixed-size buffers: vertices, indices, uniforms and "storage" (the
models' vertex arrays, copied in whenever a draw binds a different array). A lot packed with converted
PC objects (whose models carry many more vertices than the GameCube's) overflowed the 8 MiB storage
buffer, and aurora aborts on a full buffer (`simmy.exe exited with code 3`, `push` in recording.cpp).
Measured: 533 converted objects in view need 25.4 MiB of vertex arrays and 16 MiB of vertices.

- `cmake/aurora_patches/0005-frame-buffer-overflow.patch`: vertices 32 MiB, indices 8, storage 48
  (uniforms 24 and texture uploads 24 as before); and a full buffer no longer aborts: the push fails,
  the rest of that frame's draws are dropped (objects flicker for a frame) and aurora logs `frame buffer
  full` (the 1st, 2nd, 4th, 8th, ... time).
- The port logs each new high above half a buffer: `[gfx] frame buffers (high water, MiB): vertices
  ... of 32, indices ... of 8, uniforms ... of 24, vertex arrays ... of 48`.
- Copying each array once per frame (instead of once per binding) would save only about a quarter
  (measured), and the game rewrites some arrays in place between draws, so it was not done.
- `cmake/SimmyAuroraPatches.cmake` lists the patches it applied in `<aurora>/.simmy_patches`: 0005
  changes lines 0002 changed, so "reverses cleanly" can no longer tell whether 0002 is in the tree.

## Not changed, and why

- **The resource caches' GameCube limits** (the 800-entry free lists, the 24 MB-era ARAM budget for
  sound samples in `ENgcAudioSampleManager`) were left exactly as on the GameCube: they never evicted
  in the sessions measured, and with the disc cache a reload is a memory copy anyway. Nothing in the
  simulation reads memory figures from the port's heaps (the fire-code meter counts objects and
  models, `pcttarget.cpp`), and nothing here changed them.
- **The GPU pipeline cache per build.** Keeping it across builds would remove the first-session builds
  after every rebuild but brought back stale-pipeline errors before; a release build only pays it once.
  aurora can also load a bundled `initial_pipeline_cache.db` next to the exe (its log says "No bundled
  initial pipeline cache found"); generating one per release from a scripted session would remove the
  first-session cost for players too. Left for later.

## Capturing a profile on Windows

1. Start the game as usual (SimmyLauncher or `simmy.exe --iso ...`).
2. Press **F3** (or F1 -> Graphics -> Frame-hitch profiler -> On). The log starts at that moment.
   To profile from the very start, add to `simmy.ini`:
   ```
   [debug]
   perf = on
   ```
3. Play until the slowdowns have happened a few times. Note roughly when.
4. Quit the game normally (F1 -> Quit, or close the window): the summary is written at exit.
5. Send `perf_log.txt` (next to `simmy.exe`, beside `game_log.txt`) together with `game_log.txt`.

Useful A/B runs (one setting changed per session, each with the profiler on):

| Question | Run |
|---|---|
| Was it the disc? | `disc_cache = off` vs default, the first session after a reboot (or with the image on a hard disk) |
| Pacing / vsync | `frame_pacing = sleep` vs `precise` vs `vsync` with `vsync = on`; compare `late`, `acquire` and `mean |frame - 16.67 ms|` |
| GPU pipelines | the first session after installing a new `simmy.exe` vs the second: `gpu (pipelines)` hitches should only appear in the first |

What can not be measured in the development container (no GPU, Linux): D3D12 pipeline build and
shader compile times and whether they hold up the main thread, the display's real vsync timing, and
Windows' sleep behaviour. The `gpu`, `acquire` and `late` phases of a Windows `perf_log.txt` answer
those.
