// @class CasCostumes after CasSimDescriptionS2C
// The costumes (game/cascostumes.cpp): per gender a list of whole sim descriptions named after the outfit
// ("PJ", "naked", "swimsuit", "workout", "marriage", ...), sorted by name, that an outfit change copies
// over the sim's own clothes (SAnimator2::setNewModel). Loaded from binary 0x4C967A29 ("costumes.bin", a
// Recon stream of type 'CCST'). EGlobal +0x490 holds it.
#pragma once
#include <vector>

struct CasCostumes {
  CasCostumes();  // 8015F524
  ~CasCostumes();  // 8015F55C
  // Name order (the first's name before the second's; a missing one sorts first).
  static bool CompareSimDescription(CasSimDescriptionS2C* a, CasSimDescriptionS2C* b);  // 8015F5E0
  void ClearCostumesList();  // 8015F654
  // The list from the binary, else a new one. 1.
  int LoadCostumesList();  // 8015F718
  void DoStream(ReconBuffer* buffer, int version);  // 8015F948
  // The gender's costume named `name` (none found: "PJ", unless that was asked for).
  CasSimDescriptionS2C* GetSimDescription(bool male, const char* name);  // 80160040
  CasSimDescriptionS2C* GetSimDescription(bool male, unsigned int index);  // 8016010C

  // Not ported (generated declarations):
  bool SaveCostumesList();  // 8015F7FC
  void CreateNewCostumesList();  // 8015FE24
  RET_UNKNOWN UpdateSimDescription(bool, char *, CasSimDescriptionS2C *);  // 80160148
  RET_UNKNOWN UpdateSimDescription(bool, unsigned int, CasSimDescriptionS2C *);  // 80160298

  std::vector<CasSimDescriptionS2C*> m_lists[2];  // +0x00 male, +0x10 female
};
