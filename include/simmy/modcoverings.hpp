// PC mod: mods - wall and floor coverings (src/game/modcoverings.cpp; the mod.ini side is
// src/platform/pc_mods.cpp, simmy::mods_coverings).
//
// The GameCube keeps a wall side's or a floor tile's covering as a byte: its index in the build sets'
// "WallSet" (95 rows on the disc) or "FloorSet" (67) table. With coverings in the mods, the sets are
// copied into the build sets image's mod tail with room for 255 patterns, the disc's rows first; a
// covering gets one of the free pattern numbers (a "slot") when the player picks it in the catalogue or
// a saved lot uses it. The lot's save gets a palette ('Arry' resource kPaletteId: slot -> covering key,
// and the GameCube covering to show instead) that the next load maps back, to the covering's new slot
// or, when its mod is gone, to that GameCube covering. Without coverings nothing here is set up, a save
// without a palette is loaded as it is, and a palette is only read (to fall back).
#pragma once

struct cFixedWorldImpl;
struct iResFile;
struct ERQuickdata;

namespace simmy::modcov {
constexpr int kPaletteId = 0x4d43;           // 'Arry' resource id of a lot's palette ("MC")
constexpr unsigned int kModCell = 0x40000000;  // a catalogue cell's id for covering n: kModCell | n

// SetDefaults: the build sets found; with coverings, the copied sets (rows in the image's tail) replace
// `wallSet` / `floorSet`. Nothing without coverings.
void InstallSets(ERQuickdata* buildSets, const void*& wallSet, const void*& floorSet);
bool Active();                       // coverings are installed
int DiscCount(bool floor);           // the disc's patterns (the set's count without coverings)
int CoveringCount(bool floor);       // the mods' coverings of that kind
const void* CoveringRow(bool floor, int n);  // covering n's row (catalogue cells)
const unsigned short* CoveringDesc(const void* row);  // a covering row's description, else null
// The slot of covering n (assigned now if it has none: a free number, or one the lot no longer uses);
// -1 when the lot already uses 255 patterns.
int Slot(bool floor, int n);
int FloorScale(int pattern);         // tiles one picture of floor `pattern` spans (1: a disc floor)
// A pattern given as a number or as a covering's key (test aids: SIMMY_PLACE_TEST paperroom / floorroom).
int PatternFromText(bool floor, const char* text);

// cFixedWorldImpl::Load / Save (world.cpp): the palette after the world's arrays.
void AfterWorldLoad(cFixedWorldImpl* world, iResFile* file);
void AfterWorldSave(cFixedWorldImpl* world, iResFile* file);
}  // namespace simmy::modcov
