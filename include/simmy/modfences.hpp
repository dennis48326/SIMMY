// PC mod: mods - fences (src/game/modfences.cpp; the mod.ini side is src/platform/pc_mods.cpp,
// simmy::mods_fences).
//
// The GameCube keeps a tile edge's wall or fence as a style byte (TileWalls: 1 a wall, 2 / 0xC / 0xD /
// 0xE the fences, 3, 5, 6, 0xF, 0x17 doors and windows, ...) and draws a fence edge as one model chosen
// by its style (EIFenceWall). With fences in the mods, the styles kFirstStyle..kLastStyle are converted
// fences: a fence gets one of them (a "slot") when the player picks it in the catalogue or a saved lot
// uses it, and the game treats it as a fence everywhere it asks (IsFenceStyle). The lot's save gets a
// palette ('Arry' resource kPaletteId: style -> fence key and the GameCube fence to show instead) that
// the next load maps back, to the fence's new style or, its mod gone, to that GameCube fence. Without
// fences nothing is set up and no lot holds such a style (a palette, or a stray style byte in that range,
// is still mapped to a GameCube fence when a lot loads).
#pragma once

struct cFixedWorldImpl;
struct iResFile;
struct CTilePt;
struct PCTFenceData;

namespace simmy::modfence {
constexpr int kPaletteId = 0x4d46;            // 'Arry' resource id of a lot's fence palette ("MF")
constexpr unsigned int kModCell = 0x40000000;  // a Walls tab cell's id for fence n: kModCell | n
constexpr int kFirstStyle = 0x30;              // the styles converted fences take on a lot
constexpr int kLastStyle = 0xfe;

bool Active();                                 // fences are in the mods
inline bool IsModStyle(int style) { return style >= kFirstStyle && style <= kLastStyle; }
int Count();                                   // the mods' fences
const PCTFenceData* Data(int n);               // fence n's catalogue data (its style: -1 until it has one)
const unsigned short* Desc(const void* data);  // a fence's catalogue description, else null
// The style of fence n (given now if it has none: a free one, or one the lot no longer uses); -1 when
// the lot already shows every style.
int Slot(int n);
int Price(int style);                          // a converted fence's price (-1: not one)
bool Walk(int style);                          // Sims walk over it (no obstacle)
// The model of a converted fence's edge `segment` (4, 8, 0x10, 0x20) on `tile`, with its posts where the
// run ends or turns (fences with posts only there); 0 when `style` is no converted fence.
unsigned int Model(int style, int segment, const CTilePt& tile);
// A style given as a number or a fence's key (test aid: SIMMY_PLACE_TEST fenceline).
int StyleFromText(const char* text);

// cFixedWorldImpl::Load / Save (world.cpp): the palette after the world's arrays.
void AfterWorldLoad(cFixedWorldImpl* world, iResFile* file);
void AfterWorldSave(cFixedWorldImpl* world, iResFile* file);
}  // namespace simmy::modfence
