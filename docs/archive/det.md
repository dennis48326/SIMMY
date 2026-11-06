# Determinism test for lockstep netplay (SIMMY port, tree /home/claude/gfx/port, build /home/claude/gfx_build)

Diff vs base78: `det.diff` (this folder, 27 files). Scripts (this folder):
- `run.sh NAME TICKS` one run (MODE=offscreen|headless, GFX=, TWO=1, EXTRA="SIMMY_DET_..."), free play lot 13 via
  the pad sequence; writes `out/NAME.hash/.log`.
- `vary.sh REC REF TICKS [base perturb165 perturb90 noaslr bigenv jitter10 audio0]` headless replays of REC
  compared with REF (EXTRA_ALL="SIMMY_DET_BOOT=1 SIMMY_PAD_SEQ=" for boot recordings).
- `ovary.sh REC REF TICKS name:gfx...` offscreen replays with graphics settings (TWO=1 for 2P).
- `cmp.py A.hash B.hash` first divergent tick + parts; `lcmp.py A.lines B.lines` first differing object line.
- `win/det_check.ps1` (+ `win/L.rec`, `win/linux_L.hash`): the Windows check (section 6).
Recordings/hashes of the runs: `out/` (L/C/B/W .rec = 1P / 2P / boot / boot inputs; *.hash). Large dumps deleted.

## 1. How the game ticks (and what was wall-clock)

One game frame = `EApp::SystemUpdate` (802E24B8):
1. `EEngine::PreFrameUpdate` (802E2EC8): reads the frame clock (`ENgcClockMan`, 8032DA3C..8032DB3C, PC: steady_clock),
   `RetraceUpdate` (802E2D5C) turns the measured seconds into 1..15 retraces, `_dt = retraces * 1/60`
   (or a smoothed average with predictive smoothing). **So `_dt` is wall-clock time.** Then
   `ENgcControllerManager::Update` (8032DD60) reads the pads (PADRead).
2. `ESimsApp::Update` (80005BA4): `UpdateApt` (Apt/Flash UI; `updateFunc` 800100A8 advances Apt by the
   `Timer` (80076F3C) milliseconds = wall clock), `UpdateGame` -> state machines -> `ELiveMode::Update`
   (8008E4C0) -> `cSimulatorImpl::Simulate` (80141614): adds `min(_dt, 1/15 s) * speed` to an accumulator
   (max 2520 ms) and runs one `SimulateOneTick` (8014095C) per 42 ms of it (2 game seconds per tick; objects in
   id order, `TickAllObjects` 80140E20, with the tick's own tree RNG `m_randSeed`); `_dt` becomes the simulated
   frame time; then the direct-control interactors (stick walking), cameras, `EHouse::Update` (every object /
   person animator per frame by `_dt * speed`: SAnimator2 event frames, routing, motive-less movement), then
   `UpdateAudio` (cSoundPlayer, BoxX tracks, ambient score), `UpdateShaders`, `UpdateDraw` (render; poses are
   computed in the level draw, `EAnimController::Compute`, cached per `_framecount`).
3. `PostFrameUpdate`, `platform_frame_tick` (pc_main.cpp: events, 60 Hz pacing by sleep).

Consequences: the simulation is a function of the *sequence of frame times*, not just of the 42 ms ticks
(per-frame animator/routing/interactor work integrates `_dt`). A lockstep "tick" therefore has to be one
**game frame with a fixed `_dt`** (1/60 s); the 42 ms sim ticks then fall deterministically inside frames
(the accumulator is state). The UI (Apt) is also part of the lockstep state: dialogs pause/resume the
simulator (`H2DTarget::IncPause/ReleasePause` 801B8A40/801B8ACC from `G2DTarget`), selection callbacks run
game code, the pie menu queues interactions; their timing comes from Apt's clock.

Wall-clock / host-dependent inputs found (all on the main thread except where noted):
| source | where | effect on the sim |
|---|---|---|
| frame time -> `_dt`, `_time`, `_d_retraces` | `EEngine::PreFrameUpdate`/`RetraceUpdate`, `ENgcRenderer::Swap` (8033EC3C, PC RetraceCount) | everything per frame (sim speed, animators, routing, UI) |
| `EClock` (`_sysclock`, frame/cpu clocks) | e_ngcclock.cpp | Apt `Timer` (UI animation phase, dialog close tweens), key repeat, camera, dialogs' timers |
| Apt `GetTimer` (80276138) | aptactioninterpreter.cpp | ActionScript tweens (dialog hide -> `UnloadUI` 801B7000 -> sim resume) |
| `timeGetTime` in gzsndsys / ambientscoreplayer | sound | sound timing (and via the tree RNG, see 2b) |
| background-thread loads (`BackgroundImpl::ExecuteRoutine` 802E1E54) | house load, CAS init, saves | number of frames the front end/loading runs -> Apt timing phase at the house |
| audio mixer thread (`AudioFrameCallback`, 5 ms of wall time) | e_ngcaudio.cpp | voice/stream completion -> sound director/ambient score (RNG, see 2b) |
| movies (`e_ngcmovie.cpp`, steady_clock + decoder thread) | EA/intro, story ending | when the state machine goes on (not fixed, see 5) |
| F1 options overlay pauses `ESimsApp::Update` | pc_overlay.cpp `overlay_pauses_game` | a client with the overlay open stops simulating (not fixed, see 5) |
| fps cap (non-det mode: whole retraces per frame) | pc_settings.cpp | `_dt` of 2 retraces at 30 fps |
No other threads touch the simulation (resource loads are synchronous on PC; stream/movie/watchdog threads don't).

## 2. Nondeterminism sources found (and fixed)

a) **Wall-clock frame time** (class: wall-clock / render-coupled). Fix: `SIMMY_DET=1` (pc_det.cpp) makes the
   game's clock virtual: `simmy::game_clock_ns()` advances exactly 1/60 s per game frame
   (`det_frame_begin` in PreFrameUpdate); used by `ENgcClockMan` (all EClocks, so the Apt Timer, `_dt`,
   `_time`), the renderer's retrace count, Apt `GetTimer`, both sound `timeGetTime`s and the pad-sequence
   test driver. `_dt` is forced to one retrace (as SIMMY_FIXED_DT). Off by default: normal play keeps GC's
   variable `_dt`.
b) **The tree RNG (`srandSeed`, `GetNextRandomNumber` 800A64F8) was shared with the sound code**
   (class: RNG misuse, audio/threading-coupled): `cTrack::OnStartPlaying` (801177B0), `cTrack::DoCommand`
   / `Choose` (801180A4/80118074), `AmbientSoundPlayer` (800A9374, 800A983C, RandBelow),
   `AmbientScorePlayer::AddSoundBasedOnProbability` (800A8ED8). Whether/when a sound starts or finishes
   (audio device present, mixer thread progress in wall time, listener = camera position, voices) changed the
   number of draws, inside a tick (the sim's `m_randSeed`) or outside (the outer seed used by check trees run
   from menus). Seen as: audio on vs `SIMMY_AUDIO=0` differ from tick 59; replays with frame-time jitter
   differ at tick ~19979 (ambient score draw one frame earlier/later). Fix: `simmy_sound_rand()`
   (cassimstate.cpp), the same generator with its own state, for all sound code. Same distribution;
   behaviour-preserving except that sounds no longer perturb the sim's sequence (which is what GC "did" by
   accident). Always on.
c) **Host CRT `std::rand` shared by sim and UI** (class: RNG misuse + platform CRT). The sim drew
   `std::rand()` in autonomy (`TryFindBestAction` 80127114), routing (`XRoute::ConstructGoals` 80158318),
   SAnimator2 (`selectRandomIdle` 800632EC, `selectIdleOverlay` 8006BD5C, `GetCostumeName` 800675A0), wants
   (`WantFearManager::Lottery` 8014C3B8, `Neighbor::Notify` 800CAD10); the same stream was drawn by Apt
   (`Random` 80275D20, `Math.random` 8026E3E4), the per-player highlight refresh
   (`ISimInstance::HasInteractions` 80056628), CAS, the sound director (`cSoundModeManager` 800B0B00..),
   the unlock display (80077C3C). glibc and MSVC `rand()` are different generators (MSVC RAND_MAX 32767),
   and any per-client UI difference shifts the sim's draws. GC's rand (sn/libc 802432B0, disassembled from
   the ELF: `state = state*1103515245 + 12345; return state & 0x7FFFFFFF`) was one stream seeded from the
   clock by `ParticleSysOpen` (803037DC). Fix: `simmy::sim_rand()` / `simmy::ui_rand()` (pc_det.cpp) = GC's
   algorithm in two streams; the sim's starts at `SIMMY_DET_SEED` (default 1, like the port's unseeded host
   rand before; GC seeded from the clock - a netplay host would send the seed); the particle system keeps
   its own (psystem.cpp). Always on (fidelity improves: 31-bit GC values on MSVC too).
d) **Loading time decides the UI's phase at the house** (class: threading / wall-clock). The house load,
   CAS init and saves run on the background thread while the main loop runs loading-screen frames; their
   count varies, so the Apt clock's phase at the first house frame varied, and a dialog's close tween
   (`G2DTarget` state 5 -> 6, `UnloadUI` 801B7000) finished a frame earlier or later, resuming the
   simulator one tick apart (first seen: tick 233, pause flag global 0x11 and the outer tree seed).
   Fix: with SIMMY_DET, `ExecuteRoutine`/`ExecuteArgRoutine` wait for the routine (`Flush`), so loads take
   a fixed number of frames (the loading screen does not animate meanwhile in det mode only).
None found of: uninitialised reads (MALLOC_PERTURB_), pointer-ordered iteration (ASLR on/off, env size),
render coupling (window size/aspect/msaa/hud, headless vs offscreen), float differences within one build.

## 3. The harness (all off by default; pc_det.cpp, documented at its top)
- `SIMMY_DET=1` deterministic timing (above). `SIMMY_DET_SEED`, `SIMMY_DET_SPEED=<n>` (speed at the house's
  first tick), `SIMMY_DET_JITTER=<ms>` (random sleep per frame), `SIMMY_DET_NOPACE=1`, `SIMMY_DET_QUIT=<tick>`.
- `SIMMY_DET_RECORD=f` / `SIMMY_DET_REPLAY=f`: the four PADStatus per tick (text: `tick err,button,sx,sy,cx,cy,
  l,r,a,b,ext` x4), recorded after every script/soak/overlay filter, i.e. exactly what the game reads; replay
  replaces the pads entirely (neutral pads after the file). Ticks count from the house's first live-mode
  frame, or from boot with `SIMMY_DET_BOOT=1` (then the file drives the front end and loading too).
- `SIMMY_DET_HASH=f`: `tick total rng= sim= obj= per= ngh= inst=` (64-bit FNV-1a) per tick.
  `SIMMY_DET_DUMP=f` + `SIMMY_DET_DUMP_TICKS=a-b`: every hashed field, one line per object;
  `SIMMY_DET_REF=hashfile` (+`_REF_QUIT=1`): dumps the tick before and the first differing tick automatically;
  `SIMMY_DET_LINES=f`: per object running hashes every tick + the RNG values + a UI line (frame, Apt time,
  `_dt`, pause count, managed screens, dialog state) to find the first differing object (`lcmp.py`).
  `SIMMY_DET_RANDLOG=1` logs every tree `Random` draw (object, tree, node, in/out of a tick);
  `=2` (GCC/Clang) also every `GetNextRandomNumber` caller address (addr2line).
- Hash coverage: RNGs (outer tree seed, tick tree seed, sim rand); simulator globals (clock, speed, pause,
  funds...), funds/pending, tick count, accumulator, speed, tick object; for every object (module list
  order): type, attributes, sprites, temps, object data (keys + values), location, level, misc flags, slot
  contents, sim flags 1/2/4, idle status, tree stack (tree, node, stack object, prim state/data, params,
  locals, error, last result/transition, iterations), relationships; for every person: person data
  (0x100), motives + old motives, motive deltas, action queue (type, objects, entry, vars, locals, priority,
  tree, attenuation, id, flags), current-action flag, idle count, position, facing, route count, neighbour,
  SAnimator2 (routing state, pos/dir, anim state, desired state/dir, zoom ratio, event frame/fraction,
  current/overlay anim ids, time/pause mult, walk style/blend, shuffle, long idle, impatience, carry,
  pending anim events, skill anim/loop, flags, awareness, route node/result/end dir); every neighbour
  (person data 0x50, motives, friend/enemy counts, reputation, want/fear bookmarks + active slots,
  relationships); every 3D instance's matrix. Not hashed: UI/Apt state (diagnostic line only), social-mode
  flags (socialmode.cpp static map), route internals (only count), world tiles/walls/floors, camera
  (only through the animators' zoom ratio), sound/particle/UI rand streams (excluded on purpose).

## 4. Results
Scenario: free play, lot 13 (Freebers, 4 sims + NPCs), Winston controlled; input = `SIMMY_SOAK` random play
(walks, camera, interact + first menu item, B, Y, X, classic cursor, d-pad, L/R, pause menu and tabs,
buy/build mode) recorded with SIMMY_DET_RECORD, speed x5 from the first house tick (SIMMY_DET_SPEED=-2).
24000 frames = 7:00 day 22 -> 02:50 day 23 (~20 game hours; the last ~third in buy mode where the soak got
stuck), with autonomy (cook meal, eat, sleep, bath, TV, music, toilet, clean, phone), autonomous and
player socials (talk, jokes, slap, threaten, gossip, card trick, kicky bag, social mode), NPCs, wants.
2P: same with SIMMY_TWO_PLAYER=1 (pad 2 macros: walk, interact, X, build mode...), ~19.5 game hours.
"match N" = per-tick total hash identical for all N ticks compared.

Before the fixes (house-start replays, 1500 ticks):
| pair | result | cause |
|---|---|---|
| record vs replay, run alone | match 903, diverge 904 (rng) | (sound draws, 2b) |
| replay vs replay, 2 in parallel / SIMMY_DET_JITTER=40 | diverge 233 (rng, sim: pause flag) | 2d: Apt phase from load time (dialog close one tick off) |
| after virtual clock + synchronous loads, parallel/jitter | diverge 605 / 485 / 545 / 853 (rng: outer tree seed) | 2b: cTrack sound draws |
| audio on vs SIMMY_AUDIO=0 | diverge 59 (rng) | 2b |
| 24000-tick replay vs record, jitter 10 | diverge 19979 (rng) | 2b: ambient score draw |

After the fixes (final binary unless noted; reference = a plain replay of the same recording):
| run pair | result |
|---|---|
| 1P record (soak live) vs replay (file only), 24000 ticks | match 24000 (L, R0) |
| 1P replay vs replay, headless no pacing (base) | match 24000 |
| + SIMMY_DET_JITTER=10 (random 0-10 ms sleep each frame) | match 24000 |
| + 60 Hz real pacing instead of none (430 s run) | match 24000 |
| + SIMMY_AUDIO=0 (no audio device) | match 24000 |
| + MALLOC_PERTURB_=165 / =90 | match 24000 / 24000 |
| + ASLR off (setarch -R) | match 24000 |
| + 6 KB larger environment | match 24000 |
| headless vs offscreen 640x360 (1500 ticks, paced) | match 1500 |
| offscreen window=640x360 / 1024x768 aspect 4:3 | match 3000 / 3000 |
| offscreen msaa=4 | match 8000 |
| offscreen 1280x720 hud=edges | match 6698 (time-limited) |
| offscreen 1920x1080 | match 3658 (time-limited) |
| offscreen 1260x540 aspect 21:9 | match 8000 |
| 2P record vs replay, 24000 ticks (headless) | match 24000 (C, C0) |
| 2P replay: base / jitter10 / MALLOC_PERTURB_=165 / ASLR off / audio off | match 24000 each |
| 2P offscreen split=windows 640x360 vs headless reference | match 4000 |
| 2P offscreen split=wide 960x360 | match 4000 |
| 2P offscreen split=gc 640x480 | match 4000 |
| 2P offscreen split=4x3 + hud=edges | match 4000 |
| boot lockstep (SIMMY_DET_BOOT=1): record from boot vs replay with no scripts at all, 22000 ticks | match 22000 (B, B1) |
| boot replay: jitter10 / audio off / MALLOC_PERTURB_=165 / ASLR off / big env | match 22000 each |
| boot record vs replay (W, the Windows-check input), 24000 ticks; clean `env -i` run | match 24000 |
Offscreen runs are software-rendered (~12 fps), hence the shorter ones. vsync: the offscreen surface has no
vsync; it only changes wall pacing, which jitter/pacing/no-pacing cover. Real windows were not tested
(no display here); they differ from offscreen only in presentation.

Regression suite (normal, non-det mode; `$S/m16/suite.sh` pointed at /home/claude/gfx_build/simmy through
temporary copies, removed afterwards): toilet 56.9, sofa 28.3, fridge 32.0, shower 45.5, bed 31.8;
`grep -c "cancels: 1" s_cancel.log` = 1; script error/TREEASSERT = 0 (`suite_out.txt`). Toilet rerun: 56.7 with
this binary and 56.7 with main's - the 56.9 is the normal wall-clock run-to-run variance of non-det mode.

## 5. Remaining issues / what a lockstep implementation still needs
Within one build, with SIMMY_DET, nothing structural is left in what was exercised. A lockstep layer still needs:
1. **Run every client in SIMMY_DET mode** (fixed 1/60 s game time per frame) and make the lockstep tick one game
   frame: the inputs are the four 12-byte PADStatus per frame (the RECORD/REPLAY format), applied in
   `ENgcControllerManager::Update` exactly where `det_pads` does. A slow client slows everyone (no catch-up:
   `_dt` must not grow). fps cap / vsync / window / msaa / HUD / split mode then only change wall pacing and
   pictures (verified), and a client's frame rate no longer changes game speed.
2. **Start state.** The Apt UI is part of the lockstep state (dialogs pause/resume the simulator and run
   callbacks; their timing is Apt's clock since boot). Simplest correct choice, verified here
   (`SIMMY_DET_BOOT=1` replays from boot match): lockstep from boot - identical disc image, identical memory
   card contents (saves *and* options/profile), identical `simmy.ini`-independent game settings, intro
   movies skipped, controller connect/disconnect as part of the input. Starting lockstep at a later point
   (e.g. host loads a lot, clients join) would need a full state transfer *including* Apt/UI timing state
   (Timer, Apt movie frame accumulators, getTimer start, ui_rand), which does not exist today.
3. **Per-client things that currently feed the game and must become shared input or be disabled in netplay:**
   - the F1 options overlay stops `ESimsApp::Update` (pc_overlay.cpp `overlay_pauses_game`): must not pause
     the game in netplay (or be a networked pause);
   - movies (`e_ngcmovie.cpp`) end on the wall clock / decoder thread: the story's ending movie would desync;
     make movie length frame-based in det mode or treat it as a barrier;
   - memory-card operations (save/load results, card full/missing dialogs) differ per machine: host-authoritative
     result or no saving during netplay; the save itself runs synchronously in det mode;
   - `SIMMY_*` development switches must be off (SIMMY_SIM_SPEED, SOAK, PAD_SEQ, ... act on frames, not input).
4. **Cross-compiler / cross-CRT floats** (unverified, see 6): the sim uses float math throughout (motives, routing,
   animators) and some transcendental calls whose results can differ by 1 ulp between glibc and the MSVC CRT:
   `std::atan2/sin/cos/sqrt/floor` in sanimator2.cpp, `atan2/sin/cos/lround` person.cpp, `sin/cos/hypot/sqrt/
   round/floor` xrouting.cpp, `atan2/sqrt` directinteractor.cpp (stick -> direction), `acos/sqrt` camera.cpp
   (zoom ratio feeds the run speed), `sin/sqrt` esim.cpp. sqrt/floor/round are exact everywhere; sin/cos/atan2/
   acos/hypot are not guaranteed identical. Windows-only netplay (everyone runs the same simmy.exe) avoids the
   problem entirely; mixed Linux/Windows play needs the check in 6 and, if it fails, deterministic
   replacements (e.g. a small fdlibm-derived sin/cos/atan2/acos/hypot in the sim paths). Keep x64 SSE2 code
   (no x87), MSVC `/fp:precise` (default; never `/fp:fast`), no FMA contraction (`/fp:contract` off - the
   MSVC default; add `-ffp-contract=off` to GCC/Clang if a build ever enables FMA via -march).
5. **Desync detection**: send this harness's per-tick total hash (or every N ticks) and stop/resync on mismatch;
   the dump/REF tooling locates the field.
6. Hash coverage gaps (social-mode flags map, route internals, world tiles) are worth adding before relying on
   the hash for desync detection; none of them can diverge without the hashed state diverging soon after.
Estimate: the determinism side is done for same-binary play; a basic 2-player input-delay lockstep over UDP
(sync start from boot, input exchange, hash check, the overlay/movie/card items above) is roughly 1-2 weeks of
work; cross-platform (Linux<->Windows) play adds an unknown 0-1 week depending on the Windows check.

## 6. Windows-side check (MSVC build)
`win/det_check.ps1` with `win/L.rec` (input) and `win/linux_L.hash` (this Linux/Clang build's hashes). Copy the
`win` folder to the Windows machine and run (PowerShell):

    powershell -ExecutionPolicy Bypass -File det_check.ps1 -Exe C:\path\to\simmy.exe -Iso C:\path\to\TheSims2.iso

It runs the replay twice headless (`--headless --frames 400000 --iso ...` with SIMMY_DET=1, SIMMY_DET_BOOT=1,
SIMMY_DET_NOPACE=1, SIMMY_DET_SPEED=-2, SIMMY_DET_QUIT=24000, SIMMY_DET_REPLAY=L.rec, SIMMY_DET_HASH=...,
an empty SIMMY_INI, SIMMY_SKIP_INTRO=1; the second also SIMMY_DET_JITTER=10), each in a fresh folder (the memory
card is created under the working directory), ~1 min each, and prints
`Windows a vs b` (expected: match 24000 ticks) and `Linux vs Windows a` (cross-compiler: match, or the first
divergent tick). On a divergence it reruns with SIMMY_DET_REF=linux_L.hash and SIMMY_DET_DUMP, leaving
`run_c\div.dump` (all hashed fields of the tick before and the first differing ticks); with that file a Linux
run with `SIMMY_DET_DUMP_TICKS=<t-1>-<t+2>` shows the first differing field. The same exe must be built from this
tree (the hashes depend on the RNG changes). The disc image must be the same release as the extracted data
used here (USA). Equivalent Linux command (what produced linux_L.hash):
`SIMMY_INI=<empty> SIMMY_SKIP_INTRO=1 SIMMY_DET=1 SIMMY_DET_BOOT=1 SIMMY_DET_NOPACE=1 SIMMY_DET_SPEED=-2
SIMMY_DET_QUIT=24000 SIMMY_DET_REPLAY=L.rec SIMMY_DET_HASH=out.hash simmy --headless --frames 400000 --data <dir>`.

## 7. Verdict
**Viable with listed work.** With SIMMY_DET timing (fixed 1/60 s of game time per frame) and the RNG
separation, the full game - simulation, autonomy, UI, both players - is a pure function of the per-frame pad
input in this build: every perturbation that differs between players' machines (render size/aspect/MSAA/HUD/
split mode, frame pacing and jitter, audio device, memory layout) left 20+ game hours bit-identical. What is
left is netplay plumbing plus a few per-client features to neutralise (section 5: overlay pause, movies,
memory card, sync start from boot or identical start state), and one open risk: cross-compiler float/CRT
differences (section 5.4) for mixed Linux/Windows play - same-exe Windows-only play does not have it. Run the
Windows check (section 6) before designing around mixed platforms.
