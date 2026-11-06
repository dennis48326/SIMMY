# SIMMY PC port plan (option 2: non-matching decompilation)

Goal: a native Windows x64 build of The Sims 2 (GameCube, `u2_ngc` build 1.mainline_SKU1.0.63).
It does not need to match the original bytes, only to behave the same. Once it runs, add widescreen
split-screen and one window per co-op player.

## Architecture

```
            +------------------------------------------------------+
 ported     |  game/  engine/  base/  libaptz/   (C++, from raw/)  |
 (us)       +------------------------------------------------------+
            |  port/platform: AX+MIX audio on SDL, VM stub,         |
            |  GX perf stubs, SN libc -> host CRT                    |
            +------------------------------------------------------+
 reused     |  aurora (MIT): GX on D3D12/Vulkan, PAD, VI, DVD,     |
            |  CARD (.gci saves), OS, AR, THP video, SDL3 windows    |
            +------------------------------------------------------+
```

The game calls 251 distinct Dolphin SDK functions. Aurora *implements* 147 of them (59%). An earlier
estimate of 217 counted functions that are only declared in its headers. The gaps are concentrated:
OS threads, alarms and message queues (33), GX perf/peek/poke (18), AX/MIX audio (24), PS matrix
math (12), VI retrace (7), AI (6).

That matters less than it sounds. Almost every direct SDK call sits in EA's GameCube platform layer,
the 40 `e_ngc*` files (779 functions, about 5% of the game). The port **rewrites that layer for PC**
(std::thread, the host allocator, std::chrono, aurora for GX/PAD/CARD) instead of emulating the SDK
underneath it. Gameplay, engine and UI code (95%) is ported faithfully on top. Earlier gap list
(`ghidra/sdk_missing.txt`):

| Missing | Count | Plan |
|---|---|---|
| AX* (voices, ADPCM, sample-rate conversion) | 13 | Write a small AX mixer on SDL3 audio; DSP-ADPCM decode is well documented |
| MIX* (faders, pan, mute) | 11 | Thin layer on the AX shim |
| GX perf metrics, FIFO limits, breakpoints | 9 | No-op stubs |
| GXSetZScaleOffset | 1 | Map to aurora's depth range |
| VMInit / VMAlloc (ARAM-backed virtual memory) | 2 | Plain host allocation (PCs have plenty of RAM) |

Dropped entirely: `sdk/*` (100 units, replaced by aurora), `sn/*` (libc/libm/libgcc, replaced by
the host CRT), and `linkonce/*` (1,966 template/inline instances the compiler regenerates from headers).

That leaves **~503 units to port**: 269 `game`, 151 `engine`, 42 `libaptz`, 41 `base`. That is about
3.3 MB of PowerPC code, roughly 15,000 functions.

## Big technical risks (in order)

1. **64-bit pointers in loaded data.** Aurora/Dawn needs x64. Apt UI files (`AptConstFile`,
   `AptCharacterAnimation::Fixup`) are loaded straight into memory and their 32-bit offsets are
   rewritten into pointers in place. On x64 those structs need explicit loaders that read the file
   into native structs. Audit every `Fixup`, `BIG_pointer` and `getIndexPointer` path early.
2. **Endianness.** The data in `DATA/*.arc` is big-endian. Good news: `IFFResFile2::IsLittleEndian` and
   `ChainResFile::IsLittleEndian` show the resource code inherited endian handling from the PC Sims
   engine. Swap bytes at loader boundaries, never deep in gameplay code.
3. **Struct layouts.** The map gives names and parameter types, but not member variables or return
   types. Recover layouts from constructors, `operator new` sizes and field offsets in the raw
   decompilation. Start with the largest classes: `cXObjectImpl` (276 methods), `SAnimator2`,
   `cXPersonImpl`, `AptActionInterpreter`.
4. **GX vertex/texture data endianness under aurora.** Check how aurora expects array data
   (`GXSetArray`), since Metaforce converted assets up front.
5. **Threads and timing.** The game uses OS threads and alarms (`OSCreateThread`, `OSSetAlarm`).
   Aurora maps these onto host threads, but frame pacing must be verified.

## Work order

Each phase ends with something visible, so progress can be checked against Dolphin.

| Phase | Work | Done when |
|---|---|---|
| 0 | Boot the original in Dolphin (`sys/` folder) | Reaches main menu; reference for behavior |
| 1 | ✅ Project skeleton: CMake + aurora + generated declarations; stub every function | Builds, links and runs (Clang/Linux verified) |
| 2 | ✅ Boot path: `main`, global objects, heaps, threads, `EApp::Main`, engine init through the clock, file system and graphics bring-up | Window opens, game loop ticks |
| 3 | 🔶 Resource system: ✅ file system, `EResourceLoaderImpl`, `.ind`/`.arc` index and archives, managers; ✅ controllers, memory card, frame loop; ✅ storable type registry and object-graph reader; ✅ loaders for datasets, textures, shaders, fonts, animations. To do: models (with phase 4), `NghResFile`, IFF/Chain res files | Assets load without crashing |
| 4 | 🔶 Renderer: ✅ `ENgcRenderer` state commands, TEV/lighting set-up, `Execute`, `Rect`; ✅ `ENgcRenderStateCache`; ✅ `ERC`/`ENgcRC` recording and immediate mode, `EDL`; ✅ `EWindow`; ✅ `--render-test`, `--screenshot`, `--offscreen`. To do: geometry commands (`TriStrip*`, `NgcGXDisplayList`, `QuadList`, `RectList`), `E3DWindow`, models, render surfaces, frame effects | Loading screen, then a lot renders |
| 5 | Apt UI (`libaptz`, `AptActionInterpreter`, game UI targets) with 64-bit-safe loaders | Main menu works |
| 6 | Simulation: `cXObjectImpl`, `cXPersonImpl`, behavior trees, `SAnimator2`, CAS | Live mode playable |
| 7 | Audio (AX/MIX shim), saves (CARD → aurora `.gci`), movies (THP via aurora) | Feature complete |
| 8 | **Split-screen goals**: aspect fix in `ELiveMode::InitPlayerXWindow`; per-player offscreen targets via aurora, presented in two SDL windows | Two 4:3 windows in co-op |

Phase 8 is far easier on PC than in Dolphin. Aurora has custom offscreen-rendering APIs and owns the
SDL window, so each player's viewport can render to its own target and window, with its own HUD
layout.

**Display lists on PC.** GameCube `EDLEntry`s were 8 bytes: an opcode byte, three argument bytes and a
word that was either a value or a pointer; commands with inline data (`Rect`, `ClipRect`) took
several entries. A pointer no longer fits in a word, so on PC an entry is 16 bytes (`op`, `b1..b3`,
`u`/`f` for values, `p` for pointers) and inline data is a payload struct after the header entry
(`EDLRectData`, `EDLClipRectData`). Recorder (`ERC`) and executor (`ENgcRenderer`) both use these
types, so the layout is internal to the port.

**Quickdata on PC.** Quickdata images (`ERQuickdata`: every UI string and many game tables) were
loaded whole and `applyFixups` patched each 32-bit offset into a pointer. On PC the buffer keeps the
GameCube layout and stays big-endian, but each fixed-up word holds a *self-relative* offset (target
minus the word's own address; 0 = null), so a pointer field resolves from its own address with no image
base. A language blob is a slot array of string pointers plus UTF-16 strings (true of every quickdata on
the disc): the strings are made host-endian at load, and each slot gets a native 8-byte cell after the
blob that pointers to the slot are redirected to. So a field the original used as `wchar_t**` resolves to
a real `const unsigned short* const*`. Callers keep reading rows by their original offsets, through
`ERQuickdata`'s static accessors: `U32/S32/U16/S16/U8/F32(row, off)` for values, `Ptr`/`String(row, off)`
for pointer and `char*` fields, `LocSlot(row, off)` for the `wchar_t**` handle and `LocString(row, off)`
for the text, plus `TableName/TableRows/TableRowNames/TableRowName/TableRowCount/TableRowSize` for the
table returned by `getTable`/`findRow`. For example, `EGlobal::GetNewUIString` becomes
`row = qd->getRow(qd->getTable("NewUIStrings"), name)`, falling back to the row
`"deprecated_string_message"`, and then `return row ? ERQuickdata::LocSlot(row, 0) : _emptyString`,
where `_emptyString` is a `const unsigned short* const*` to `u""`. Text is UTF-16, so the game's 16-bit
`wcs*` calls need 16-bit helpers on PC (`wchar_t` is 32 bits on Linux). `--qd-test` prints a few strings.

**Apt movies on PC.** A Flash movie is a 'FLSH' resource in `flashes.arc` holding a BIG archive:
`<name>.apt` (characters, timelines, ActionScript bytecode), `<name>.const` (constant table and the
offset of the movie in the .apt), `<name>.geo` ("Apt PCGL geometry file": pre-triangulated shapes) and
`NNN.tgq` bitmaps (TXFL C8 textures, header little-endian). `ERFlash`/`FlashBigFile` load every entry
into its own block (`locatbig.cpp` reads the BIG directory). On GC Apt then used the blocks in place:
`AptCharacterAnimation::Resolve/Fixup`, `AptMovie::resolve`, `AptActionInterpreter::resolveStream` and
the game's `_loadGeometry` added the base to each 32-bit offset (and stored AptValue pointers into the
bytecode's constant pools). On PC the data model is split in two:
- **Structure is parsed into native structs** (`include/simmy/classes/aptfile.hpp`, built by
  `AptFileImage::Build` in `src/libaptz/aptfile_pc.cpp`): `AptCharacterMovie` (+ `AptCharacterAnimation`
  with the character table, imports, exports), sprites with their `AptMovie` timeline, frames,
  `AptControl*` (PlaceObject2, RemoveObject2, Action, InitAction, FrameLabel, ...), shapes, images,
  edit/static text, fonts, buttons, morphs, and `AptConstFile` items. Fields keep the GC order with GC
  offsets in comments; ids that Link turns into pointers get both an id and a pointer field. Values
  are host-endian. The runtime parts of Fixup/Link (aux callbacks for textures, rendering units and
  sounds, the label hash, imports) are to run on top of these structs.
- **Strings and bytecode stay in the retained .apt/.const images** and are pointed into (bytes need no
  swapping). Bytecode is never patched: the interpreter decodes big-endian aligned arguments and
  resolves offsets on access (string/array offsets against `AptFileImage::AptAt`, constant-pool indices
  through `AptConstFile::items[i].resolved`, created once per item), so every running stream needs its
  `AptFileImage`, reachable from the root animation instance in `LocalContextT` (GC +0x28).
`SIMMY_APT_TEST=s2c_root,s2c_main_menu,...` loads and summarises movies; all 36 named in s2c_root's
constants parse (two are "Apt Data:6", same layout).

**Apt runtime on PC.** The ActionScript side is an idiomatic AVM1 with Apt's semantics taken from the
original (`aptruntime.hpp`, `aptplayer.hpp`, `src/libaptz/`): `AptVar` values, `AptObject` property
maps with getters/setters and flags, a mark-and-sweep `AptGC`, the bytecode interpreter
(`aptactioninterpreter.cpp`: Flash 5-7 ops plus Apt's dictionary/register extensions 0x59-0x76 and
0xA1-0xB8 reading the retained .apt/.const images), display lists of `AptCIH` instances (timelines,
masks, edit text, clip events), and the built-in classes (`aptbuiltins.cpp`). The original classes'
own methods (AptValue, AptObject, AptCIH, AptActionInterpreter, ...) are not ported one by one; their
addresses are listed in `src/libaptz/apt_original_methods.cpp`. The game side is ported as in the
original: `aptauxpceorgl.cpp` (load, geometry, textures, drawing through the ERC, strings with the
game font), `AptViewer`, the UI command tables and targets (UIRouter, FlowGotoTarget, UIQDTarget,
UIDB, UIScreenManager, MMUTarget).

**Testing without a GPU.** `--offscreen` uses aurora's headless surface; in a Linux container Dawn
falls back to Mesa llvmpipe (OpenGL ES), which renders correctly, colours included. (An earlier "all
black" result was the port skipping `GXInit`, which sets up GX's register shadows; `ENgcRenderer::InitGX`
calls it now, as the original did.)

**State machines.** The game runs as nested state machines (`StateMachine` is itself a
`StateMachineState`). States that are not ported yet are still created by their machine (the
generated stand-ins get their id assigned by `NewState<>` in thesims.cpp), so the flow reaches them
and the log shows the next stub. Texture v coordinates run up the screen: the game's images are
stored bottom row first and its 2D rects pass uv0 = (0, 1), uv1 = (1, 0).

## How each unit gets ported

1. Start from `raw/<unit>.c` (Ghidra output with real names) and `port/include_gen/<unit>.hpp`.
2. Rewrite as idiomatic C++ in `src/<unit>.cpp`: give methods real return types, turn `*(int *)(this + 0x2c)`
   into named members, and replace SN/EA intrinsics.
3. Callers into the SDK compile against aurora's `dolphin/*.h` unchanged.
4. Keep the original address in a comment above each function (`// 80003FEC`) for cross-reference
   with the assembly, Ghidra and Dolphin's debugger.
5. Unported functions stay as `UNIMPLEMENTED()` stubs that log their name on first call, so the
   game runs as far as it can and the log shows what to do next.

## Tools in this repo

| Path | What |
|---|---|
| `ghidra/scripts/ApplySimsSymbols.java` | Applies all 18,547 function and 4,520 data names (with C++ namespaces) and the r13 small-data base |
| `ghidra/scripts/ExportUnits.java` | Decompiles everything into `raw/`, one file per original source file |
| `tools/gen_headers.py` | Header skeletons per source file from demangled signatures (442 headers) |
| `tools/ghidra_symbols.py` | Builds the Ghidra inputs from the dtk config |
| Ghidra language | Gekko/Broadway extension (paired-single support), needed for the math code |
