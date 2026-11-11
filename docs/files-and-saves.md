# Files, resources and saves

## Overview

The game reads everything from a **GameCube disc image** (or an extracted folder for development)
through one read-only virtual file layer, loads big-endian **resources** from `DATA/*.arc` archives by
32-bit id, and saves to an emulated **memory card** (aurora CARD, Dolphin-compatible `.gci` files). The
neighbourhood file format (`DEFAULT.NGH` on the disc, "Sims 2 Save" on the card) is the same fixed-section
layout in both places.

## The virtual file layer ([`data_path.cpp`](../src/platform/data_path.cpp), [`gcm_disc.cpp`](../src/platform/gcm_disc.cpp))

- Source chosen at startup by `open_game_data` (pc_main.cpp): `--data <dir>` > `--iso <file>` / `iso=` in
  simmy.ini > `SIMMY_DATA`; none or an unusable image: a message (a message box on Windows) and exit 1.
- **Disc**: `GcmDisc` checks boot.bin (rejects RVZ/WIA/GCZ/CISO/WBFS/NKit/Wii/other games, truncated images),
  reads the FST once into a hash map of lower-case paths -> {offset, size}. `HostFile` does positional reads
  (`pread` / `ReadFile` with OVERLAPPED; UTF-8 paths via `CreateFileW`), so any number of threads can read one
  handle. Any region works (the game ID is only logged).
- **Folder**: the same through the host file system, case-insensitive.
- Required files are checked at open: `DATA/index.ind`, `quickdat.arc`, `textures.arc`, `shaders.arc`,
  `fonts.arc`, `datasets.arc`, `default.ngh` (missing movies/audio/samples only warn).
- API ([`platform.hpp`](../include/simmy/platform.hpp)): `data_open/close/exists/size/read/seek/tell/error`,
  `data_read_at` (positional, lock-free: the music reader and THP decoder threads use it),
  `data_read_file`. Sequential reads use a 64 KB read-ahead. `SIMMY_IO_STATS=1` prints opens/reads at exit.
  With `disc_cache` (default `auto`: on with 8 GB of RAM) a background thread reads the 22 archives the
  game loads resources from (825 MB; not movies.arc / audiostr.arc) into RAM in 4 MiB chunks at start;
  a read whose chunks are in is served from RAM (the same bytes), the rest from the disc. This removed
  the in-play disc stalls; see [performance.md](performance.md).
- The game's `ENgcFile` ([`e_ngcfile.cpp`](../src/base/e_ngcfile.cpp)) sits on a `DataFile`; write modes are
  refused. `EFileSystem`/`EFile` ([`e_filesystem.cpp`](../src/base/e_filesystem.cpp), [`e_file.cpp`](../src/base/e_file.cpp)) are ported as written.
- Test images: `python tools/make_test_iso.py <folder> <out.iso>` (game ID `SIMT01`).

## Resources

- Archives: `DATA/index.ind` (+ `.arc` files) index every resource by manager and id. Names hash to ids with
  `EChecksum::ComputeSymbol` ([`e_checksum.cpp`](../src/base/e_checksum.cpp)). `tools/resdump.py` lists,
  dumps and extracts.
- `EResourceManager` subclasses (one per kind: textures, shaders, models, animations, fonts, datasets,
  quickdata, flashes, samples, sound events, ...; [`e_resourceman.cpp`](../src/engine/e_resourceman.cpp))
  with `AddRef`/`DelRef`/`GetRef`. Loads are synchronous on PC ([`e_resloader.cpp`](../src/engine/e_resloader.cpp)); the game
  polls `IsLoaded`/`GetRef`, so async requests just complete at once. `SIMMY_LOG_LOADS=1` logs every load.
- Every resource starts with `EDataHeader`; object graphs are read by `EFile`'s `operator>>` /
  `ReadStructure` with the `ETypeInfo` registry ([`e_storable.cpp`](../src/base/e_storable.cpp), [`e_typeinfo.cpp`](../src/base/e_typeinfo.cpp)).
- **Quickdata** (`ERQuickdata`, [`e_rquickdata.cpp`](../src/engine/e_rquickdata.cpp)): table images kept
  big-endian; fixed-up pointer words hold **self-relative offsets**; strings are made host-endian UTF-16 at
  load. Read rows only through `ERQuickdata::U32/S32/U16/S16/U8/F32/Ptr/String/LocSlot/LocString(row, off)`
  ([PORTING.md](../PORTING.md) "Quickdata on PC"). UI text is UTF-16: use the 16-bit helpers, not
  `wchar_t` (32-bit on Linux).
- **Endianness rule**: data stays big-endian in loaded images and is swapped at the read (`simmy::be16/be32`
  in [`port.hpp`](../include/simmy/port.hpp), the `ERQuickdata` accessors). Never byte-swap deep in gameplay
  code, never store a pointer into a 32-bit field of loaded data.

## The neighbourhood file ([`nghresfile.cpp`](../src/game/nghresfile.cpp), [`nghsave.cpp`](../src/game/nghsave.cpp))

`NghResFile` (`Globs_pNghResFile`) holds the save as fixed-size sections (layout from nghsave.cpp's header):

| Offset | Size | Section |
|---|---|---|
| 0x000000 | 0x4000 | card banner, icon, comments; options at 0x2000 (`OptionsRecon`, [`optionsrecon.cpp`](../src/game/optionsrecon.cpp)) |
| 0x004000 | 0x2000 | validity mark (0xDEADBEEF once complete) |
| 0x006000 | 0x2000 | "host": `TATT`, each house's `SIMI` (simulator) and `HOUS` |
| 0x008000 | 0xA000 | "both": `NGBH`, `NBRS`, `FAMI`, `User` ([cas-and-neighbourhood.md](cas-and-neighbourhood.md)) |
| 0x012000 | 36 x 0x800 | family thumbnails (`thmb`) |
| 0x024000 | 16 x 0x10000 | houses: `Arry` (world tiles, run-length), `objt`, `ObjM`, `DUMP` |
| 0x124000 / 0x125000 | 4 x 0x400 each | registry sections `Uid1..4`, `Uix1..4` (`SerializeUIGameData`, [`tsupport.cpp`](../src/game/tsupport.cpp)) |

A section = 0x1C-byte header + LZSS-compressed tables and data. On the card headers/tables are
**big-endian** (GC order), in `DEFAULT.NGH` little-endian; resource bytes are little-endian in both
(`NghResFile::IsLittleEndian`). Checksum = CRC-32 of the header as stored (checksum field 0) + the
compressed data. The LZSS encoder reproduces the original bytes. Streams inside resources use
`ReconBuffer`/`ReconBuilder` ([`recon.cpp`](../src/game/recon.cpp), [`world.hpp`](../include/simmy/classes/world.hpp)).

**Card cache.** All writes go to `MemoryCardCache` first; the card file is written from it. A new game
copies the 16 houses of `DEFAULT.NGH` into the cache (`CopyDefaultNghToCache`), which then is the source
for house loads (so free-play edits to unplayed lots persist).

## Saving and loading

- **Requests** run through `MUStateMachine` ('MUST', [`mustates.cpp`](../src/game/mustates.cpp)):
  `MURequest(kRequest...)` (load game 1, load house 2, autosaves 3/4, save existing game 5, transition
  autosave 7, ...), polled with `MUPollForResult` (1 done, 2 failed/cancelled, 3 refused: quit pending).
- **Save from the pause menu**: O2T Save -> M2M slot select -> `SaveGameTarget::PerformSaveGame` step 0
  (simulator paused, lot value computed, pending funds committed, `NeighborhoodImpl::SaveHouse`
  (800D1110, [`lotsave.cpp`](../src/game/lotsave.cpp)): the family takes the lot's funds, the house's resources
  rewritten by the PC stand-in `HouseSaveFile` (`SIMI`, `HOUS`, `cFixedWorldImpl::Save` `Arry`,
  `ObjectModuleImpl::Save` `objt` + `ObjM` (each object's `ReconStream` incl. its tree stack) + `DUMP`),
  then the neighbourhood) -> the background `SimsMemCardWrap::SaveGame` -> `NghResFile::WriteToMemoryCard`.
- **Load (Continue)**: `LoadGameTarget` -> `SimsMemCardWrap::LoadGame` -> `MemoryCardCache::LoadCacheFromMemoryCard`
  (whole file, per-section CRC) -> `OptionsRecon::ReadIn` -> `NghResFile::ReadFromMemoryCard` ->
  `NeighborhoodImpl::Load` -> PreGameState skips CAS -> the family's house.
- Lot leave paths save with request 5 (flows 5/10) or 7 (taxi), see [cas-and-neighbourhood.md](cas-and-neighbourhood.md).
- Memory card code: [`memcardscreens.cpp`](../src/game/memcardscreens.cpp) (`SimsMemCardWrap`, `M2MTarget`,
  load/save targets), [`e_ngcmemorycard.cpp`](../src/engine/e_ngcmemorycard.cpp) (on aurora CARD). Every
  GC card dialog (no card, damaged, no space, overwrite, ...) is wired.

## Where files go

| What | Where |
|---|---|
| Saves | aurora CARD: `<SDL pref path>/SIMMY/USA/Card A/*.gci` (Windows `%APPDATA%\SIMMY\USA\Card A\`; Linux the XDG data dir). `CARDInit("G4ZE", "69")` is fixed, so PAL discs use USA too. **Headless** runs (no aurora) put the card in `./USA/Card A` under the working directory |
| `simmy.ini` | next to the exe, or `--config` / `SIMMY_INI` |
| Crash / watchdog reports | next to the exe ([shutdown-and-crashes.md](shutdown-and-crashes.md)) |
| GPU pipeline cache | `<pref path>/cache_<exe size>_<mtime>` |
| Screenshots, WAV dumps, portrait dumps | paths given by the developer (cwd) |
| The disc image / data folder | **never written** |
| Mods (PC mod) | read only, from `mods=` / `mods` next to the exe ([mods.md](mods.md)); a save names a mod object by GUID only |

## Gotchas

- aurora's `CARDUnmount` always reports "no card"; `e_ngcmemorycard.cpp` wraps it (`Unmount`) so card
  operations do not fail afterwards. Keep using the wrapper.
- Saves written by the port are not guaranteed loadable by the GC (`HouseImpl` is a stand-in; strings are
  written GC-style 0xFFFE big-endian UTF-16 while `DEFAULT.NGH` has 0xFEFF little-endian; both read).
- Tests that save need a fresh card dir per run (delete the `USA` folder) and must not run from the build
  directory headless (the card lands there).
- `--data` beats the disc image even when `iso=` is set (logged).
- PC mod: with a mod folder, a resource id the archives lack is looked up in the mods (`EResourceLoaderImpl::DoLoadResource`,
  `simmy::mods_resource`), and every quickdata image gets 64 KB of zeroed tail (`ERQuickdata::ModTail`) where mod rows live
  ([mods.md](mods.md)). Without a mod folder neither happens.
