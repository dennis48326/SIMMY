// Port of game/casclothingunlocks.cpp: the player family's clothing and tattoo unlocks. See
// casclothingunlocks.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

// 8015E9D4
CasClothingUnlocks::CasClothingUnlocks() {
  std::memset(m_body, 0, sizeof m_body);
  std::memset(m_tattoo, 0, sizeof m_tattoo);
}

// 8015EAB8
CasClothingUnlocks::~CasClothingUnlocks() {}

// 8015EB20
void CasClothingUnlocks::Unlock(bool male, eBodyPartS2C part, unsigned int index, bool markNew) {
  if (!CheckBodyIndex(index)) return;
  unsigned char& f = m_body[male ? 1 : 0][part][index];
  f &= 0xfd;
  if (markNew) f |= 4;
}

// 8015EBA4
void CasClothingUnlocks::Unlock(bool male, eTattooTextureTypeS2C type, unsigned int index, bool markNew) {
  if (!CheckTextureIndex(index)) return;
  unsigned char& f = m_tattoo[male ? 1 : 0][type][index];
  f &= 0xfd;
  if (markNew) f |= 4;
}

// 8015EC28
void CasClothingUnlocks::Lock(bool male, eBodyPartS2C part, unsigned int index) {
  if (!CheckBodyIndex(index)) return;
  unsigned char& f = m_body[male ? 1 : 0][part][index];
  f |= 2;
  f &= 0xfb;
}

// 8015ECA0
void CasClothingUnlocks::Lock(bool male, eTattooTextureTypeS2C type, unsigned int index) {
  if (!CheckTextureIndex(index)) return;
  unsigned char& f = m_tattoo[male ? 1 : 0][type][index];
  f |= 2;
  f &= 0xfb;
}

// 8015ED18
void CasClothingUnlocks::ClearRecentlyUnlocked(bool male, eBodyPartS2C part, unsigned int index) {
  if (CheckBodyIndex(index)) m_body[male ? 1 : 0][part][index] &= 0xfb;
}

// 8015ED80
void CasClothingUnlocks::ClearRecentlyUnlocked(bool male, eTattooTextureTypeS2C type, unsigned int index) {
  if (CheckTextureIndex(index)) m_tattoo[male ? 1 : 0][type][index] &= 0xfb;
}

// 8015EDE4
unsigned char CasClothingUnlocks::GetFlags(bool male, eBodyPartS2C part, unsigned int index) const {
  return CheckBodyIndex(index) ? m_body[male ? 1 : 0][part][index] : 0;
}

// 8015EE4C
unsigned char CasClothingUnlocks::GetFlags(bool male, eTattooTextureTypeS2C type, unsigned int index) const {
  return CheckTextureIndex(index) ? m_tattoo[male ? 1 : 0][type][index] : 0;
}

// 8015EEB0
bool CasClothingUnlocks::IsLocked(bool male, eBodyPartS2C part, unsigned int index) const {
  return CheckBodyIndex(index) && (m_body[male ? 1 : 0][part][index] >> 1 & 1);
}

// 8015EF1C
bool CasClothingUnlocks::IsLocked(bool male, eTattooTextureTypeS2C type, unsigned int index) const {
  return CheckTextureIndex(index) && (m_tattoo[male ? 1 : 0][type][index] >> 1 & 1);
}

// 8015EF84
bool CasClothingUnlocks::IsRecentlyUnlocked(bool male, eBodyPartS2C part, unsigned int index) const {
  return CheckBodyIndex(index) && (m_body[male ? 1 : 0][part][index] >> 2 & 1);
}

// 8015EFF0
bool CasClothingUnlocks::IsRecentlyUnlocked(bool male, eTattooTextureTypeS2C type, unsigned int index) const {
  return CheckTextureIndex(index) && (m_tattoo[male ? 1 : 0][type][index] >> 2 & 1);
}

// 8015F058
bool CasClothingUnlocks::CheckBodyIndex(unsigned int index) const { return index < 0x28; }

// 8015F068
bool CasClothingUnlocks::CheckTextureIndex(unsigned int index) const { return index < 0x1e; }

// 8015F078
// Everything locked, then the starting set (group 0) unlocked without "new" marks.
void CasClothingUnlocks::ClearUnlocks() {
  std::memset(m_body, 2, sizeof m_body);
  std::memset(m_tattoo, 2, sizeof m_tattoo);
  UnlockGroup(0, false);
}

// 8015F0D4
// From version 0x10: the 2 x 32 x 40 body bytes, then 2 x 8 x 30 tattoo bytes. An older stream being
// read leaves the starting set (ClearUnlocks).
void CasClothingUnlocks::DoStream(ReconBuffer* buffer, int version) {
  if (version < 0x10) {
    if (buffer->m_mode == ReconBuffer__Mode::kRead) ClearUnlocks();
    return;
  }
  for (auto& gender : m_body)
    for (auto& part : gender)
      for (unsigned char& f : part) buffer->Recon8(reinterpret_cast<signed char*>(&f), 1);
  for (auto& gender : m_tattoo)
    for (auto& type : gender)
      for (unsigned char& f : type) buffer->Recon8(reinterpret_cast<signed char*>(&f), 1);
}

// 8015F1CC
void CasClothingUnlocks::UnlockGroup(unsigned int group, bool markNew) { MarkGroup(group, false, markNew); }

// 8015F1F4
void CasClothingUnlocks::LockGroup(unsigned int group) { MarkGroup(group, true, false); }

// 8015F21C
// Every style (both genders, the 26 body parts x 40 styles, the 3 tattoo types x 30 textures) whose
// unlock group is `group`, locked or unlocked.
void CasClothingUnlocks::MarkGroup(unsigned int group, bool lock, bool markNew) {
  CasSimPartsS2C parts;
  for (int g = 0; g < 2; ++g) {
    const bool male = g != 0;
    parts.SetGender(male);
    for (int part = 0; part < 0x1a; ++part) {
      for (unsigned int i = 0; i < 0x28; ++i) {
        if (static_cast<unsigned int>(parts.GetUnlockGroupNum(eBodyPartS2C{part}, i)) != group) continue;
        if (!lock) Unlock(male, eBodyPartS2C{part}, i, markNew);
        else Lock(male, eBodyPartS2C{part}, i);
      }
    }
    for (int type = 0; type < 3; ++type) {
      for (unsigned int i = 0; i < 0x1e; ++i) {
        if (static_cast<unsigned int>(parts.GetUnlockGroupNum(eTattooTextureTypeS2C{type}, i)) != group) continue;
        if (!lock) Unlock(male, eTattooTextureTypeS2C{type}, i, markNew);
        else Lock(male, eTattooTextureTypeS2C{type}, i);
      }
    }
  }
}

// PC: which groups (below 0x100) still have a locked style.
void CasClothingUnlocks::GetLockedGroups(bool locked[0x100]) const {
  for (int g = 0; g < 0x100; ++g) locked[g] = false;
  CasSimPartsS2C parts;
  for (int g = 0; g < 2; ++g) {
    const bool male = g != 0;
    parts.SetGender(male);
    for (int part = 0; part < 0x1a; ++part)
      for (unsigned int i = 0; i < 0x28; ++i) {
        const unsigned int group = static_cast<unsigned int>(parts.GetUnlockGroupNum(eBodyPartS2C{part}, i));
        if (group < 0x100 && IsLocked(male, eBodyPartS2C{part}, i)) locked[group] = true;
      }
    for (int type = 0; type < 3; ++type)
      for (unsigned int i = 0; i < 0x1e; ++i) {
        const unsigned int group = static_cast<unsigned int>(parts.GetUnlockGroupNum(eTattooTextureTypeS2C{type}, i));
        if (group < 0x100 && IsLocked(male, eTattooTextureTypeS2C{type}, i)) locked[group] = true;
      }
  }
}

bool CasClothingUnlocks::IsUnportedPortSave() const {
  for (const auto& gender : m_body)
    for (const auto& part : gender)
      for (unsigned char f : part)
        if (f != 2) return false;
  for (const auto& gender : m_tattoo)
    for (const auto& type : gender)
      for (unsigned char f : type)
        if (f != 2) return false;
  return true;
}
