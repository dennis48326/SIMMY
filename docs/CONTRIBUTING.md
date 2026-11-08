# Keeping the docs current

These pages are only useful if they match the code. Every fix or feature updates them in the same
change.

## What to update

| You changed | Update |
|---|---|
| Boot, the frame, threads, state machines, platform hooks | [architecture.md](architecture.md) |
| Trees, primitives, scopes, people, autonomy, motives, wants, routing, social | [simulation.md](simulation.md) |
| World, rooms, house build, buy/build tools, placement | [build-buy-and-world.md](build-buy-and-world.md) |
| CAS, neighbourhood, families, free play, lot changes, portraits | [cas-and-neighbourhood.md](cas-and-neighbourhood.md) |
| Pads, interactors, commands, sim switching, keyboard/mouse | [input-and-control.md](input-and-control.md) |
| Apt runtime, targets, screens, flows, HUD, dialogs, widescreen HUD | [ui.md](ui.md) |
| Renderer, aurora, resolution, split screen, effects, aurora patches | [rendering.md](rendering.md) |
| Cameras | [camera.md](camera.md) |
| Sound | [audio.md](audio.md) |
| File layer, resources, saves, paths | [files-and-saves.md](files-and-saves.md) |
| Anything per player or co-op | [coop-and-players.md](coop-and-players.md) (and its "hard-coded to two" list) |
| RNG use, timing, input entry points, hashed state | [determinism-and-netplay.md](determinism-and-netplay.md) |
| Exit path, crash handling | [shutdown-and-crashes.md](shutdown-and-crashes.md) |
| SimmyLauncher | [launcher.md](launcher.md) |
| A `SIMMY_*` variable, a test approach | [testing.md](testing.md) (the variable table) |
| Ported a stub listed as a gap, found a new gap, kept a GC quirk on purpose | [known-gaps.md](known-gaps.md) |
| User-visible settings/controls | the repo [README](../README.md) and `simmy.ini.example`, not these pages |

A new system that fits no page: add a page, link it from [README.md](README.md)'s table.

## Where findings go

- **Durable knowledge** (how something works, why a PC deviation exists, a gotcha, a GC address worth
  knowing): the matching page, in the section it belongs to. Short; link to the source file.
- **Code-local detail** (field offsets, the exact GC algorithm): a comment above the function, with its
  address. Pages point at the file instead of repeating it.
- **Session working notes** (what you tried, test commands, before/after numbers): optional, as a new file
  in [archive/](archive/README.md) with a dated row in its index. Never link pages to archive notes as the
  source of truth; distil what stays true into the pages.
- When a fix changes behaviour described on a page, **rewrite** the old statement; do not append a
  correction below it.

## How to write a page entry

- State what the code does now, not its history ("ported in session N" belongs in the archive).
- Verify each claim against the code (grep the address or name) before writing it; if unsure, say so.
- Name functions with their GC address the first time (`ELiveMode::Update` 8008E4C0); link files with
  relative paths (`../src/game/livemode.cpp`).
- Mark PC deviations as such and say why.
- Keep tables and lists tight; small ASCII diagrams only where they clarify flow.

## Rules for code changes (repeated here because they cause most regressions)

- **MSVC**: C++20 only, no VLAs, no GNU extensions (statement expressions, `__builtin_*` without
  guards, zero-length arrays), no reliance on `wchar_t` being 16 or 32 bits, include every header you use
  (`<algorithm>`, `<cstring>`, `<cmath>`, `<mutex>`...), avoid huge string literals (MSVC's 64 KB limit),
  no `/fp:fast`. Code is built and tested with Clang on Linux; the Windows build uses MSVC.
- **Address markers**: `// XXXXXXXX` above a function marks it ported (the generator stops stubbing it).
  Put the marker only on real ports of that function; a PC helper that stands in for one should say
  `GC XXXXXXXX` and leave the member to its stub, but then **callers of the member still hit the stub**.
- **Determinism**: no `std::rand`, no wall time in simulation code; the right RNG stream per subsystem;
  new input only at `ENgcControllerManager::Update`; new simulation state added to the det hash.
- **Per-player UI**, **no static destructors / one exit path**, **big-endian data read through
  accessors**, **ids not pointers in 32-bit game data slots**: see [README.md](README.md#rules-for-new-code-summary-details-on-each-page).
- **New `SIMMY_*` switches** are off by default and documented in [testing.md](testing.md).

## Port version

The port's version lives in one place: [`include/simmy/version.hpp`](../include/simmy/version.hpp).
For each delivery change `SIMMY_PORT_BUILD` (e.g. `"build df"`) and `SIMMY_PORT_DATE`
(`"YYYY-MM-DD"`); `SIMMY_PORT_VERSION` ("SIMMY port - build df (2026-11-08)") is made from them.
It shows in the game window's title (window 2's too), SimmyLauncher's title bar and home page (top
right), the first log lines of both (`[simmy] SIMMY port - ...`, `[launcher] SIMMY port - ...`, and
each launch in `simmy_launcher_log.txt`), and `simmy --version` / `SimmyLauncher --version` print it.
Keep it plain ASCII (a hyphen, not an em dash). The launcher's `.rc` file version (1.0.0.0) is the
Windows file version, not this one.

## Checking links

After editing, check that every relative link resolves, e.g.:

```
python3 - <<'EOF'
import os, re, glob
for page in glob.glob('docs/*.md'):
    for target in re.findall(r'\]\(([^)#\s]+)', open(page).read()):
        if '://' in target: continue
        path = os.path.normpath(os.path.join(os.path.dirname(page), target))
        if not os.path.exists(path): print(page, '->', target)
EOF
```
