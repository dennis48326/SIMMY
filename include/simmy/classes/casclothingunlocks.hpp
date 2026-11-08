// @class CasClothingUnlocks after CasSimDescriptionS2C eBodyPartS2C eTattooTextureTypeS2C ReconBuffer
// The player family's clothing and tattoo unlocks (game/casclothingunlocks.cpp, 0xBE0 bytes, kept at
// PlayerFamilyImpl +0xC8): a flag byte per gender, body part (32 slots, 26 used) and style (40), then
// per gender, tattoo type (8 slots, 3 used) and texture (30). Flag 2: locked; flag 4: unlocked since CAS
// last showed it ("new"). ClearUnlocks locks everything, then unlocks group 0 (the starting set; each
// style's group is CasSimPartsS2C::GetUnlockGroupNum). Fashion goals (TryCompleteGoal kind 1) unlock
// group n. CAS's style lists show the flags (CasSimRendererDynamic::GetIndexes).
#pragma once

struct CasClothingUnlocks {
  CasClothingUnlocks();  // 8015E9D4
  ~CasClothingUnlocks();  // 8015EAB8
  void Unlock(bool male, eBodyPartS2C part, unsigned int index, bool markNew);  // 8015EB20
  void Unlock(bool male, eTattooTextureTypeS2C type, unsigned int index, bool markNew);  // 8015EBA4
  void Lock(bool male, eBodyPartS2C part, unsigned int index);  // 8015EC28
  void Lock(bool male, eTattooTextureTypeS2C type, unsigned int index);  // 8015ECA0
  void ClearRecentlyUnlocked(bool male, eBodyPartS2C part, unsigned int index);  // 8015ED18
  void ClearRecentlyUnlocked(bool male, eTattooTextureTypeS2C type, unsigned int index);  // 8015ED80
  unsigned char GetFlags(bool male, eBodyPartS2C part, unsigned int index) const;  // 8015EDE4
  unsigned char GetFlags(bool male, eTattooTextureTypeS2C type, unsigned int index) const;  // 8015EE4C
  bool IsLocked(bool male, eBodyPartS2C part, unsigned int index) const;  // 8015EEB0
  bool IsLocked(bool male, eTattooTextureTypeS2C type, unsigned int index) const;  // 8015EF1C
  bool IsRecentlyUnlocked(bool male, eBodyPartS2C part, unsigned int index) const;  // 8015EF84
  bool IsRecentlyUnlocked(bool male, eTattooTextureTypeS2C type, unsigned int index) const;  // 8015EFF0
  bool CheckBodyIndex(unsigned int index) const;  // 8015F058
  bool CheckTextureIndex(unsigned int index) const;  // 8015F068
  void ClearUnlocks();  // 8015F078
  void DoStream(ReconBuffer* buffer, int version);  // 8015F0D4
  void UnlockGroup(unsigned int group, bool markNew);  // 8015F1CC
  void LockGroup(unsigned int group);  // 8015F1F4
  void MarkGroup(unsigned int group, bool lock, bool markNew);  // 8015F21C
  void PrintUnlocks(CasSimDescriptionS2C* desc) const;  // 8015F390 (dev print; not ported)

  // PC: a save the port wrote before the unlocks were ported holds every byte locked and never ran
  // UnlockGroup(0) (no GC state looks like that: group 0 is always unlocked). True for such a block.
  bool IsUnportedPortSave() const;
  // PC: which groups (below 0x100) still have a locked style (for the load fix-up above).
  void GetLockedGroups(bool locked[0x100]) const;

  unsigned char m_body[2][0x20][0x28];    // +0x000 per gender (0 female), part, style
  unsigned char m_tattoo[2][8][0x1E];     // +0xA00 per gender, tattoo type, texture
};
