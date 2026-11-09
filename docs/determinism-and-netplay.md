# Determinism and netplay

## Overview

With **`SIMMY_DET=1`** the whole game (simulation, autonomy, UI, both players) is a pure function of the
per-frame pad input (plus the keyboard-and-mouse record) for a given binary: 20+ game hours of soak play
replayed bit-identically across render sizes, aspect, MSAA, HUD and split modes, frame jitter, no audio
device, memory perturbation and ASLR (Linux/Clang). This is the basis for lockstep netplay and for exact
regression replays. Normal play is **not** deterministic (wall-clock `_dt`), on purpose: it keeps GC timing.

The harness is [`pc_det.cpp`](../src/platform/pc_det.cpp) (variables documented at its top); hooks:
`det_frame_begin` (`EEngine::PreFrameUpdate`), `det_pads` (`ENgcControllerManager::Update`),
`det_live_update` (`ELiveMode::Update`), `det_frame_end` (`platform_frame_tick`).

## Why normal play is not deterministic

One game frame (`EApp::SystemUpdate`) measures wall time into `_dt` (1..15 retraces). The simulator ticks
every 42 ms of `_dt x speed`, and per-frame work (direct-control walking, animators, routing, cameras) and
the UI (Apt advances by wall-clock milliseconds; dialogs pause/resume the simulator when their tweens end)
integrate it. So the simulation depends on the sequence of frame times, not just on ticks. Other
wall-clock/host inputs found: background-thread loads (number of loading frames -> UI phase at the house),
the audio mixer thread (sound completion), movies (decoder thread), the F1 overlay pausing the game, fps cap.

## What `SIMMY_DET` changes

| Change | Where | Always on? |
|---|---|---|
| Virtual clock: `simmy::game_clock_ns()` advances exactly one retrace per frame; used by `ENgcClockMan` (all EClocks, Apt Timer, `_dt`, `_time`), the renderer's retrace count, Apt `GetTimer`, the sound clocks, the pad-sequence driver; `_dt` = 1/60 | pc_det.cpp, e_engine.cpp | det only |
| Background routines waited for (`Flush` after `ExecuteRoutine`): loads take a fixed number of frames | [`background.cpp`](../src/engine/background.cpp) | det only |
| **Separate RNG streams** (below) | pc_det.cpp, cassimstate.cpp, psystem.cpp | always |

### RNG streams (the rules for new code)

| Stream | API | Used by |
|---|---|---|
| Tree RNG | `GetNextRandomNumber` (800A64F8): the simulator's seed inside a tick, an outer seed outside | behaviour trees (`Random` primitive, goodloc, ...) |
| Simulation | `simmy::sim_rand()` (GC libc rand: `state * 1103515245 + 12345`, low 31 bits), seed `SIMMY_DET_SEED` (default 1) | autonomy, routing, animator idles/costumes, wants lottery |
| UI | `simmy::ui_rand()` | Apt `Random`/`Math.random`, highlight refresh (`HasInteractions`), CAS (`RangedRandInt`), unlock display, the sound mode manager (boxx.cpp) |
| Sound | `simmy_sound_rand()` (tree generator, own state) | cTrack, ambient players |
| Particles | psystem's own | particle systems |

GC had one libc stream for everything (seeded from the clock) and shared the tree RNG with sound; any
per-client UI or audio difference shifted the simulation. **Never call `std::rand`/`rand`** (glibc and
MSVC differ; MSVC's RAND_MAX is 32767), never draw the simulation's stream from UI or sound code or vice
versa, and never read wall time, SDL state or thread progress in simulation code.

## The harness

| Variable | Effect |
|---|---|
| `SIMMY_DET=1` | Deterministic timing (above) |
| `SIMMY_DET_SEED=<n>` | The simulation RNG seed (also outside det mode) |
| `SIMMY_DET_RECORD=<file>` | Per tick: the four `PADStatus` as the game reads them (after scripts/soak/overlay), text `tick err,button,sx,sy,cx,cy,l,r,a,b,ext` x4, plus ` m flags,cx,cy,rot,zoom,panx,pany` (floats as `%a`) when the `KbmTick` is non-zero |
| `SIMMY_DET_REPLAY=<file>` | From tick 1 the pads (and KbmTick) are the file's; live pads and scripts are ignored; neutral after the end |
| `SIMMY_DET_BOOT=1` | Ticks count from the first game frame (record/replay/hash cover front end and loading); otherwise tick 1 = live mode's first house frame |
| `SIMMY_DET_HASH=<file>` | Per tick: `tick total rng= sim= obj= per= ngh= inst=` (64-bit FNV-1a) |
| `SIMMY_DET_DUMP=<file>` + `SIMMY_DET_DUMP_TICKS=a-b` | Every hashed field, one line per object |
| `SIMMY_DET_REF=<hash file>` (`_REF_AFTER`, `_REF_QUIT`) | Compare with a reference run; dump the tick before and the first differing ticks; log `[det] DIVERGED` |
| `SIMMY_DET_LINES=<file>` | Per tick per object running hashes + RNG values + a UI line (frame, Apt time, `_dt`, pause count, screens, dialog state) to find the first differing object |
| `SIMMY_DET_RANDLOG=1` / `=2` | Log every tree `Random` draw / every `GetNextRandomNumber` caller address (GCC/Clang) |
| `SIMMY_DET_SPEED=<n>` | Simulator speed set at the house's first tick (0, -1, -2, -3) |
| `SIMMY_DET_JITTER=<ms>`, `SIMMY_DET_NOPACE=1`, `SIMMY_DET_QUIT=<tick>` | Random sleep per frame (pacing noise), no pacing, quit after a tick |

**Hash coverage**: RNG states; simulator globals, funds, tick count, accumulator, speed, tick object; per
object (module order): type, attributes, sprites, temps, object data, location, level, flags, slot
contents, sim flags, idle status, tree stack (frames, nodes, params, locals, prim state); per person:
person data, motives (+old, deltas), action queue, animator state, position, facing, route count,
neighbour; per neighbour: person data, motives, counts, reputation, wants/fears, relationships; every 3D
instance's matrix. **Not hashed**: UI/Apt state (diagnostic line only), social-mode flags (socialmode.cpp
map), route internals, world tiles/walls/floors, the passive influence map and items (rebuilt from
the objects; their effect is in the hashed motives and object trees), camera (except via animators' zoom
ratio), sound/particle/UI RNGs. Add new simulation state to `Hash` in pc_det.cpp when you add it to the game.

## How to check determinism (generic recipe)

1. Record: `SIMMY_DET=1 SIMMY_DET_BOOT=1 SIMMY_SKIP_INTRO=1 SIMMY_DET_RECORD=r.rec SIMMY_DET_HASH=a.hash`
   plus some input (`SIMMY_SOAK=<seed>` random play, or a pad sequence), `--headless` or `--offscreen`,
   an empty `SIMMY_INI`, a fresh card folder, `SIMMY_DET_QUIT=<ticks>`.
2. Replay: same but `SIMMY_DET_REPLAY=r.rec SIMMY_DET_HASH=b.hash` and no input scripts; vary what should
   not matter (`SIMMY_DET_JITTER=10`, `SIMMY_AUDIO=0`, `MALLOC_PERTURB_`, graphics settings, split mode).
3. Compare the hash files line by line; on a divergence rerun with `SIMMY_DET_REF=a.hash` (+ `SIMMY_DET_DUMP`)
   and diff the dumps / `SIMMY_DET_LINES` output to find the first differing field.

A small compare script (first differing tick and which parts differ) is worth adding to `tools/`
([testing.md](testing.md)).

## Rules for deterministic code

- Simulation state may depend only on: the previous state, the per-frame input (pads + KbmTick), and
  `_dt` (which det mode fixes). Not on: wall time, frame pacing, render size, audio, thread timing, pointer
  values or iteration order of pointer-keyed containers (`std::map<Ptr,...>` is fine to look up, not to
  iterate where order affects the simulation).
- New input goes through `ENgcControllerManager::Update` before `det_pads`, or into `KbmTick`.
- Background work that changes simulation state must be awaited deterministically (det mode flushes).
- Floats: x64 SSE2, no FMA contraction, no fast-math: CMakeLists.txt sets `-ffp-contract=off -fno-fast-math`
  (GCC/Clang) and `/fp:precise` (MSVC; never `/fp:fast`, `/fp:contract` or `/arch:AVX2` contraction) for
  the whole build, aurora included. Plain `+ - * /`, `sqrt`, `floor/ceil/round/trunc/fmod` and float<->int
  casts are then IEEE-exact and identical everywhere.
- **Transcendentals: always `simmy::dm::`** ([`detmath.hpp`](../include/simmy/detmath.hpp)), never
  `std::sin`/`cos`/`tan`/`asin`/`acos`/`atan`/`atan2`/`exp`/`log`/`log10`/`pow` (or the `f` variants) in
  src/base, engine, game or libaptz. The C libraries' results differ in the last bits (glibc's are correctly
  rounded; MSVC's UCRT picks FMA3 code paths per CPU): a Linux-vs-Windows replay (build di, AMD) first
  differed at tick 2145 in a sim's model matrix (`sin` of its facing, 0.147888437 vs 0.147888422) and, from
  tick 2516, in a walking sim's direction (`atan2f` of the route leg) and position. `simmy::dm` is the
  GameCube's own libm ([`detmath.cpp`](../src/platform/detmath/detmath.cpp): SN Systems' libm.a = newlib's
  float fdlibm, ported from the disassembly with its fused multiply-adds done in portable code), so it is
  both GC-faithful and bit-identical on every compiler and CPU. Only float versions exist (the GC had no
  others); double arguments do not compile (write `static_cast<float>`). `simmy --mathcheck [file]` hashes
  100k fixed inputs per function: the value must be `3a67a7608e988c00` on every build (Linux GCC and
  Clang, any `-O`, `-march=native` included; Windows: checked by the det_check script). It also prints the
  C library's hash for comparison (differs per platform). Remaining host-dependent float code is PC-only
  (launcher, overlay, split-screen layout, audio mixer), not part of the simulation.
- Sorts with ties: use `simmy::SnQsort` ([`sn_qsort.hpp`](../include/simmy/sn_qsort.hpp), the game's BSD
  qsort) where GC used `qsort`, not `std::sort` (its tie order is implementation-defined).

## Remaining work for lockstep netplay

1. Run every client in det mode; the lockstep tick = one game frame; inputs = 4 x PADStatus (+ KbmTick)
   per frame, applied where `det_pads` is. A slow client slows everyone (no catch-up).
2. Start state: lockstep from boot (verified) with identical disc, identical memory card contents and
   intro skipped; joining later needs a state transfer **including Apt/UI timing state**, which does not
   exist.
3. Neutralise per-client features: the F1 overlay pauses `ESimsApp::Update` (`overlay_pauses_game`);
   movies end on the wall clock (`e_ngcmovie.cpp`); memory-card results differ per machine; `SIMMY_*`
   dev switches act on frames.
4. Desync detection: send the per-tick total hash (or every N ticks); add the unhashed state above first.
5. Cross-compiler floats: the Windows check (det_check.ps1, kept with the recording outside the repo: it runs
   `--mathcheck` against the Linux value, then replays L.rec twice and against the Linux hashes with a
   per-part breakdown) showed, before detmath, MSVC self-consistent (jitter included) and rng/sim/ngh equal
   to Linux over 24000 ticks, with inst/per/obj differing from the C library's trig. With detmath the
   Linux reference hashes changed (inst from tick 2145, per from 2277: the GameCube's libm differs from
   glibc's in last bits; rng/sim/obj/ngh unchanged); the next Windows run should match them exactly.
6. Old house-start recordings can diverge after front-end changes (the front end before tick 1 is not
   recorded); boot recordings (`SIMMY_DET_BOOT=1`) are the robust kind.
