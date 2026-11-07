// Port of game/simmemory.cpp: what a person remembers of another. Each neighbour's relationship row
// towards another holds, in its variable 2, eight memories of 3 bits each (0..7); the object module
// reads and writes them by neighbour ids, the person by person. Primitive 0x3F (TryMemory,
// objectsim.cpp) is how trees use them.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 8013FCC8
RET_UNKNOWN GetIndividualMemory(int memories, int index) { return memories >> (index * 3 & 0x3f) & 7; }

// 8013FCDC
void SetIndividualMemory(int& memories, int index, int value) {
  memories = (memories & ~(7 << index * 3)) | value << index * 3;
}

// 8013FD04
// Neighbour `from`'s relationships, keyed by neighbour `to` (the row made three variables long).
void SetupMatrixAndKey(int from, int to, RelMatrix*& matrix, int& key) {
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  Neighbor* n = hood ? hood->FindNeighborByID(from) : nullptr;
  matrix = n ? n->GetRelations() : nullptr;
  key = to;
  auto* rel = static_cast<RelMatrixImpl*>(matrix);
  if (rel && rel->GetArraySize(to) < 3) rel->SetArraySize(key, 3);
}

// 8013FDF0
RET_UNKNOWN cXPersonImpl::GetSimMemory(cXPerson* other, int index) {
  return Globs_pObjectModule ? Globs_pObjectModule->GetSimMemory(GetPersonData(0x1f), other->GetPersonData(0x1f), index) : 0;
}

// 8013FE84
void cXPersonImpl::SetSimMemory(cXPerson* other, int index, int value) {
  if (Globs_pObjectModule) Globs_pObjectModule->SetSimMemory(GetPersonData(0x1f), other->GetPersonData(0x1f), index, value);
}

// 8013FF20
int cXPersonImpl::GetDominantMemory(cXPerson* other, int level) {
  if (!Globs_pObjectModule) return -1;
  return static_cast<int>(Globs_pObjectModule->GetDominantMemory(GetNeighborID(), static_cast<cXPersonImpl*>(other)->GetNeighborID(), level));
}

// 8013FFB4
RET_UNKNOWN ObjectModuleImpl::GetSimMemory(int from, int to, int index) {
  RelMatrix* m = nullptr;
  int key = 0;
  SetupMatrixAndKey(from, to, m, key);
  if (!m) return 0;
  return GetIndividualMemory(static_cast<RelMatrixImpl*>(m)->GetValue(key, 2), index);
}

// 80140028
// GC: then the module hears of the access (RelationshipAccessed, vtable 0x1AC: nothing).
RET_UNKNOWN ObjectModuleImpl::SetSimMemory(int from, int to, int index, int value) {
  RelMatrix* m = nullptr;
  int key = 0;
  SetupMatrixAndKey(from, to, m, key);
  if (!m) return 0;
  auto* rel = static_cast<RelMatrixImpl*>(m);
  int memories = rel->GetValue(key, 2);
  SetIndividualMemory(memories, index, value);
  rel->SetValue(key, 2, memories);
  return 0;
}

// 80140144
// The first memory at least `level` (-1: none).
RET_UNKNOWN ObjectModuleImpl::GetDominantMemory(int from, int to, int level) {
  RelMatrix* m = nullptr;
  int key = 0;
  SetupMatrixAndKey(from, to, m, key);
  if (!m) return -1;
  const int memories = static_cast<RelMatrixImpl*>(m)->GetValue(key, 2);
  for (int i = 0; i < 8; ++i)
    if (GetIndividualMemory(memories, i) >= level) return i;
  return -1;
}
