// PC mod: mods - doors (src/game/moddoors.cpp; the mod.ini side is src/platform/pc_mods.cpp, an
// [object] with door = 1; the rows are made by ObjectFolderImpl::AddModObjects, modobjects.cpp).
//
// On the GameCube a door is a portal object: a group of type-8 parts, one on each side of the wall per
// tile of its width, whose definition gives the wall it stands in a style (+0x5C: 3, "a door is here":
// the router and the collision map let sims through) and a row of the wall models (+0xA2: g_wallTable,
// the wall with the door's hole). A converted door is a copy of a GameCube door (its trees, wall style,
// slots and animations) showing the PC model, and its parts may carry their own wall models (wall_cut:
// the hole the PC door's wall mask cuts). Doors need no style of their own: they keep the GameCube's
// door style 3, so they never meet the converted fences' styles (0x30..0xFE).
//
// Saves name object types by GUID: a lot saved with converted doors gets a palette ('Arry' resource
// kPaletteId: each converted door type on the lot and the GameCube door type it copies), and a load
// without the door's mod gives its objects that GameCube type (ObjectFolderImpl::PrepareForModuleLoad),
// so the door stays a door (the wall keeps its door style and a door model) instead of becoming the
// placeholder the load removes. Without a palette nothing changes.
#pragma once

#include <vector>

struct iResFile;
struct cXObject;

namespace simmy::moddoor {
constexpr int kPaletteId = 0x4d44;  // 'Arry' resource id of a lot's door palette ("MD")

// A converted door's type (lead or part) and the GameCube type it copies (AddModObjects).
void Register(unsigned int guid, unsigned int fallback);
// The wall model a converted door's part gives the wall on its tile's side (end-cap variant 0..8); 0:
// none (the definition's g_wallTable row).
unsigned int WallCut(cXObject* portal, int variant);

// ObjectFolderImpl::PrepareForModuleSave: the palette of the converted door types with save ids.
void SavePalette(iResFile* file);
// ObjectFolderImpl::PrepareForModuleLoad: the palette read (before the type table), a saved type the
// game lacks noted by the type table's reader, and the save ids of those the palette names given their
// GameCube type in the id -> GUID table.
void LoadPalette(iResFile* file);
void NoteUnknown(unsigned int guid, short saveId);
void ApplyFallbacks(std::vector<unsigned int>& saveGuids);
}  // namespace simmy::moddoor
