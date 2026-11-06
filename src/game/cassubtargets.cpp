// Port of the CAS screens' targets (game/targets_cas.cpp: CASBodyTarget, CASMiscTarget,
// CASFashionTarget, CASGeneticsTarget, CASSelectionTarget, CASTattooTarget, CASMorphTarget) and the
// genetics (game/casgenetics.cpp). See cassubtargets.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

namespace {
const unsigned short* Text(const char* row) { return EGlobal::NewUIText(row); }

void CopyText(unsigned short* dst, const unsigned short* src, size_t max = 0x1000) {
  size_t i = 0;
  for (; src && src[i] && i < max - 1; ++i) dst[i] = src[i];
  dst[i] = 0;
}

char* MallocString(const char* text) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  std::snprintf(out, 0x20, "%s", text ? text : "");
  return out;
}

unsigned short* MallocText(const unsigned short* text) {
  size_t n = 0;
  while (text && text[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  for (size_t i = 0; i < n; ++i) out[i] = text[i];
  out[n] = 0;
  return out;
}

// Posts a change of the edited sim: next (mode 0) or previous (1).
void PostChange(CasMediator* mediator, int change, int mode) {
  if (!mediator) return;
  CasEventChangeSimS2C event;
  event.m_change = change;
  event.m_mode = mode;
  mediator->PostEvent(event);
}

CASTarget* Cas() { return _gd.m_casTarget; }

// "<name>" -> the command target's functors.
template <class T>
void InstallGet(T* t, const char* name) {
  t->m_getTable->InstallCommand(name, [t](char* v) { return t->GetVariable(v); }, t);
}
template <class T>
void InstallSet(T* t, const char* name) {
  t->m_setTable->InstallCommand(name, [t](char* n, char* v) { t->SetVariable(n, v); }, t);
}
template <class T>
void InstallLoc(T* t, const char* name) {
  t->m_locTable->InstallCommand(name, [t](char* v) { return t->GetLocalizable(v); }, t);
}
// The UIObjectBase destructor part of the targets (inlined on GC).
void UnInstall(UIObjectBase* t) {
  if (!t->m_installed) return;
  t->m_getTable->UnInstallAllCommands(t);
  t->m_setTable->UnInstallAllCommands(t);
  t->m_locTable->UnInstallAllCommands(t);
  t->m_updateTable->UnInstallEntry(t);
  t->m_drawTable->UnInstallEntry(t);
  if (UI2D::s_pInstance) UI2D::s_pInstance->UnInstallAllEntries(const_cast<char*>(t->m_name));
  UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(t->m_name));
}

// {anonymous}::kCGEPlumbBobLightDir / kCARPlumbBobLightDir (80487DB4 / 80487DC0, set by targets_cas.cpp's
// static init)
const float kCGEPlumbBobLightDir[3] = {0.57735026f, 0.57735026f, -0.57735026f};
const float kCARPlumbBobLightDir[3] = {0.57735026f, 0.57735026f, -0.57735026f};

// A plumb bob's lights: one directional light in the player's colour (PlumbBob::s_plumbBobParms), the
// ambient a fraction of it (inlined on GC).
void SetPlumbBobLights(ELights& lights, int player, const float* dir) {
  lights.m_numPoint = 0;
  lights.m_numDir = 1;
  const float* color = &g_plumbBobParms.m_colorScale[(player & 1) * 3];
  for (int i = 0; i < 3; ++i) {
    lights.m_dir[0].m_color[i] = color[i];
    lights.m_dir[0].m_dir[i] = dir[i];
    lights.m_ambient[i] = color[i] * g_plumbBobParms.m_field24;
  }
}

ERShader* AddRefShader(unsigned int id) { return static_cast<ERShader*>(_shaderman.EResourceManager::AddRef(id, nullptr, 0)); }
ERTexture* AddRefTexture(unsigned int id) { return static_cast<ERTexture*>(_textureman.EResourceManager::AddRef(id, nullptr, 0)); }
}  // namespace

// 80177D54
void UIObjectBase::AptViewer_CallFunction(char* function, char* unused, char* target, int argc, char** argv) {
  if (auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer)) viewer->NewCallFunction2(function, unused, target, argc, argv);
}

// ===================================================================================================
// CasGenetics

// 80161984 (CasGenetics::Grandparent::Grandparent, defaulted in the header)
// 80161998
CasGenetics__Grandparent::~CasGenetics__Grandparent() {
  delete m_desc;
  m_desc = nullptr;
  if (m_portrait) {
    m_portrait->DelRef();
    m_portrait = nullptr;
  }
}

// 801603EC (CasGenetics::CasGenetics, defaulted in the header)
// 80160424
CasGenetics::~CasGenetics() { ClearGrandparentsList(); }

namespace {
// kSkinBlendWeights (803DF02B): per pair of skin colours (low, high), the chance in % of each child
// skin colour (6 of them).
const signed char kSkinBlendWeights[6][6][6] = {
    {{99, 0, 0, 0, 1, 0}, {59, 40, 0, 0, 1, 0}, {25, 0, 74, 0, 1, 0}, {10, 0, 59, 30, 1, 0}, {75, 0, 0, 0, 25, 0}, {50, 0, 0, 0, 0, 50}},
    {{0, 0, 0, 0, 0, 0}, {0, 99, 0, 0, 1, 0}, {0, 25, 74, 0, 1, 0}, {0, 10, 59, 30, 1, 0}, {0, 75, 0, 0, 25, 0}, {0, 50, 0, 0, 0, 50}},
    {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 89, 10, 1, 0}, {0, 0, 50, 49, 1, 0}, {30, 0, 60, 0, 10, 0}, {0, 0, 50, 0, 0, 50}},
    {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 99, 1, 0}, {24, 0, 50, 25, 1, 0}, {0, 0, 0, 50, 0, 50}},
    {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 100, 0}, {0, 0, 0, 0, 50, 50}},
    {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 100}},
};
// kBodyTypeBlendWeights (803DF103): the same for the three body types.
const signed char kBodyTypeBlendWeights[3][3][3] = {
    {{60, 20, 20}, {40, 40, 20}, {40, 20, 40}},
    {{0, 0, 0}, {35, 50, 15}, {50, 25, 25}},
    {{0, 0, 0}, {0, 0, 0}, {35, 15, 50}},
};
// zodiacPersonalityValues (803DEFEF): the personality points of the twelve signs.
const unsigned char kZodiacPersonalityValues[12][5] = {
    {8, 3, 3, 5, 6}, {5, 4, 8, 5, 3}, {7, 3, 3, 4, 8}, {3, 6, 4, 6, 6}, {10, 3, 4, 4, 4}, {2, 5, 3, 9, 6},
    {8, 7, 6, 2, 2}, {5, 3, 3, 6, 8}, {3, 4, 7, 2, 9}, {4, 5, 8, 7, 1}, {4, 6, 7, 4, 4}, {3, 7, 3, 5, 7},
};

// A weighted pick of 100.
int WeightedPick(const signed char* weights, int count) {
  const int roll = static_cast<int>(GetNextRandomNumber() % 100) + 1;
  int sum = 0, pick = 0;
  for (int i = 0; i < count; ++i) {
    if (sum < roll && roll <= sum + weights[i]) pick = i;
    sum += weights[i];
  }
  return pick;
}
}  // namespace

// 801604A8
// The child: the parents' skin colour and body type mixed by the blend weights, a morph from either,
// parts at random, the personality of a random sign.
void CasGenetics::BlendSimDescriptions(CasSimDescriptionS2C& a, CasSimDescriptionS2C& b, CasSimDescriptionS2C& out,
                                       bool male) {
  out.ClearSim(false);
  out.m_male = male ? 1 : 0;
  out.m_skinColor = BlendSkinColor(a.m_skinColor, b.m_skinColor);
  if (out.m_skinColor == 5) out.m_head = 1;
  ChooseRandomBodyPart(eBodyPartS2C(2), out, 0);
  ChooseRandomBodyPart(eBodyPartS2C(7), out, 0);
  ChooseRandomBodyPart(eBodyPartS2C(0xf), out, 0);
  ChooseRandomBodyPart(eBodyPartS2C(0x10), out, 0);
  ChooseRandomBodyPart(eBodyPartS2C(0x13), out, 0x19);
  ChooseRandomBodyPart(eBodyPartS2C(0), out, 0x19);
  ChooseRandomBodyPart(eBodyPartS2C(0x12), out, 0x32);
  ChooseRandomBodyPart(eBodyPartS2C(0x14), out, 0x19);
  ChooseRandomBodyPart(eBodyPartS2C(0x15), out, 10);
  ChooseRandomBodyPart(eBodyPartS2C(static_cast<int>(GetNextRandomNumber() % 3) + 0x16), out, 0x19);
  out.m_bodyType = BlendBodyType(a.m_bodyType, b.m_bodyType);
  out.SetBodyTypeMorphValues();
  out.m_morphHead = BlendMorphValues(a.m_morphHead, b.m_morphHead);
  out.m_morph108 = BlendMorphValues(a.m_morph108, b.m_morph108);
  out.m_morph10C = BlendMorphValues(a.m_morph10C, b.m_morph10C);
  out.m_morph110 = BlendMorphValues(a.m_morph110, b.m_morph110);
  out.m_morph114 = BlendMorphValues(a.m_morph114, b.m_morph114);
  ChoosePersonalityTraits(out);
}

// 80160694
signed char CasGenetics::BlendSkinColor(signed char a, signed char b) {
  signed char lo = static_cast<signed char>(a % 6), hi = static_cast<signed char>(b % 6);
  if (hi < lo) std::swap(lo, hi);
  if (lo < 0) lo = 0;  // PC: GC indexes with a negative remainder
  return static_cast<signed char>(WeightedPick(kSkinBlendWeights[lo][hi], 6));
}

// 80160788
signed char CasGenetics::BlendBodyType(signed char a, signed char b) {
  signed char x = a < 0 ? 0 : a > 2 ? 2 : a;
  signed char y = b < 0 ? 0 : b > 2 ? 2 : b;
  if (y < x) std::swap(x, y);
  return static_cast<signed char>(WeightedPick(kBodyTypeBlendWeights[x][y], 3));
}

// 801608A0
float CasGenetics::BlendMorphValues(float a, float b) { return GetNextRandomNumber() % 100 > 0x31 ? b : a; }

// 80160900
void CasGenetics::ChoosePersonalityTraits(CasSimDescriptionS2C& desc) {
  const int sign = static_cast<int>(GetNextRandomNumber() % 12);
  for (int i = 0; i < 5; ++i) desc.m_personality[i] = kZodiacPersonalityValues[sign][i];
}

// 80160974
// A random style (and texture) of the part, of those offered in CAS and allowed for the gender; parts
// with a keep chance are left off (index 0) unless the roll beats it.
void CasGenetics::ChooseRandomBodyPart(eBodyPartS2C part, CasSimDescriptionS2C& desc, int keepChance) {
  const int roll = static_cast<int>(GetNextRandomNumber() % 100) + 1;
  bool optional = false;
  const int p = part;
  if (p == 2) {
    if (desc.m_hat != 0) return;
  } else if (p == 3) {
    if (desc.m_hat == 0) return;
  } else if (p == 7 || p == 0xf || p == 0x10) {
  } else if (p == 0x13) {
    optional = true;
    if (keepChance < roll) {
      desc.SetBodyPartIndex(part, 0);
      desc.SetBodyPartTextureIndex(part, 0);
      desc.m_hairUnderHat = 0;
      return;
    }
  } else if (p == 0x12) {
    optional = true;
    if (keepChance < roll) {
      desc.SetBodyPartIndex(part, 0);
      return;
    }
  } else if (p == 0 || (p >= 0x14 && p <= 0x18)) {
    optional = true;
    if (keepChance < roll) {
      desc.SetBodyPartIndex(part, 0);
      desc.SetBodyPartTextureIndex(part, 0);
      return;
    }
  } else {
    return;
  }
  CasSimPartsS2C parts;
  parts.SetGender(desc.m_male != 0);
  const unsigned int count = parts.GetNumBodyModels(part);
  if (!count) return;
  int tries = 0, index = 0;
  bool ok = false;
  do {
    index = static_cast<signed char>(GetNextRandomNumber() % count);
    ++tries;
    ok = parts.GetUnlockGroupNum(part, static_cast<unsigned int>(index)) == 0;
    if (p == 0xf) {
      if (desc.m_male == 0 ? index == 7 : index == 8) ok = false;
    } else if (p == 7) {
      if (desc.m_male == 0 ? (index == 4 || FemaleInnerLayerTorsoLocked(static_cast<signed char>(index))) : index == 3)
        ok = false;
    } else if (p == 0x10) {
      if (index == 9) ok = false;
    } else if (p == 0x12 && desc.m_male == 0) {
      if (FemaleFacialFeatureLocked(static_cast<signed char>(index))) ok = false;
    }
    if (optional && index == 0) ok = false;
    if (tries > 0x19) ok = true;
  } while (!ok);
  if (tries > 0x19) index = 0;
  desc.SetBodyPartIndex(part, static_cast<signed char>(index));
  if (p != 0x12) {
    const unsigned int textures = parts.GetNumBodyTextures(part, static_cast<unsigned int>(index));
    desc.SetBodyPartTextureIndex(part, static_cast<signed char>(textures ? GetNextRandomNumber() % textures : 0));
  }
  if (p == 0x10) {
    desc.SetShoeBoottopDisplayedFlag(parts);
  } else if (p > 0x10) {
    if (p == 0x13) {
      desc.SwitchHatToFirstAvailableStyle(parts);
      desc.m_hairUnderHat = 1;
    } else if (p == 0x16) {
      desc.m_musicPlayer = desc.m_beltExtra = 0;
    } else if (p == 0x17 || p == 0x18) {
      desc.m_musicPlayer = desc.m_beltExtra = 1;
    }
  } else if (p == 3) {
    desc.m_hairUnderHat = 1;
    desc.m_hairStyle = 0;
  } else if (p == 2) {
    desc.m_hairUnderHat = 0;
    desc.m_hairStyle = 0;
  } else if (p == 7) {
    desc.SwitchInnerLayerSleeveToFirstAvailableStyle(parts);
    desc.SwitchInnerLayerTorso(desc.InnerLayerHalfTorsoRequired(parts));
  } else if (p == 0xf) {
    desc.SwitchLowerBodyToFirstAvailableStyle(parts);
  }
}

// 80160D78
void CasGenetics::ClearGrandparentsList() {
  for (auto& list : m_grandparents) {
    for (CasGenetics__Grandparent* g : list) delete g;
    list.clear();
  }
}

// 80160E38
// The saved grandparents (binary 0x29b32cf6, a Recon stream), else new ones.
int CasGenetics::LoadGrandparentList(bool reload) {
  if (!m_grandparents[0].empty()) {
    if (!reload) return 1;
    ClearGrandparentsList();
  }
  EResource* saved = _binaryman.EResourceManager::AddRef(0x29b32cf6u, nullptr, 0);
  if (saved) {
    SIMMY_STUB("CasGenetics::LoadGrandparentList: ReconLoadObject<CasGenetics> (the saved grandparents)", 0x80160E38);
    saved->DelRef();
  }
  CreateNewGrandparentList();
  return 1;
}

// 801614BC
// 26 grandmas and 26 grandpas with default descriptions, named "grandma"/"grandpa" and their number.
void CasGenetics::CreateNewGrandparentList() {
  ClearGrandparentsList();
  for (int male = 0; male < 2; ++male) {
    for (unsigned int i = 0; i < 0x1a; ++i) {
      auto* g = new CasGenetics__Grandparent;
      g->m_desc = new CasSimDescriptionS2C;
      g->m_desc->m_male = male;
      char first[0x20], last[0x20];
      std::snprintf(first, sizeof first, "%s", male ? "grandpa" : "grandma");
      std::snprintf(last, sizeof last, "%i", i);
      for (int c = 0; c < 0x20; ++c) {
        g->m_desc->m_firstName[c] = static_cast<unsigned char>(first[c]);
        g->m_desc->m_lastName[c] = static_cast<unsigned char>(last[c]);
        if (!first[c] && !last[c]) break;
      }
      m_grandparents[male].push_back(g);
    }
  }
}

// 8016170C
// Each grandparent's portrait texture ("af_grandparent_<n>", "am_grandparent_<n>").
int CasGenetics::LoadGrandparentPortraits() {
  if (m_grandparents[0].empty() || m_grandparents[1].empty()) return 0;
  for (int male = 0; male < 2; ++male)
    for (size_t i = 0; i < 0x1a && i < m_grandparents[male].size(); ++i) {
      const std::string name = GetGrandparentPortraitName(male != 0, static_cast<unsigned int>(i));
      m_grandparents[male][i]->m_portrait = AddRefTexture(EResourceManager::CalcId(const_cast<char*>(name.c_str())));
    }
  return 1;
}

// 801617FC
CasGenetics__Grandparent* CasGenetics::GetGrandparent(bool male, unsigned int index) {
  const auto& list = m_grandparents[male ? 1 : 0];
  return index < list.size() ? list[index] : nullptr;
}

// 80161840
int CasGenetics::UpdateGrandparentDescription(bool male, unsigned int index, CasSimDescriptionS2C& desc) {
  CasGenetics__Grandparent* g = index < 0x1a ? GetGrandparent(male, index) : nullptr;
  if (!g || !g->m_desc) return 0;
  *g->m_desc = desc;
  return 1;
}

// 801618F0
std::string CasGenetics::GetGrandparentPortraitName(bool male, unsigned int index) {
  char name[0x40];
  std::snprintf(name, sizeof name, "%s_grandparent_%i", male ? "am" : "af", index);
  return name;
}

// 80161A00
int CasGenetics::FemaleInnerLayerTorsoLocked(signed char index) { return index == 8 || index == 0xb; }
// 80161A20
int CasGenetics::FemaleFacialFeatureLocked(signed char index) {
  return static_cast<unsigned int>(index - 6) < 2 || index == 9 || index == 10 || (index >= 0xc && index <= 0xf);
}

// ===================================================================================================
// CASBodyTarget

// 801964BC
CASBodyTarget::CASBodyTarget() { AttachTables("CASBodyTarget"); }
// 801968F4
CASBodyTarget::~CASBodyTarget() { UnInstall(this); }
// 80196A50
void CASBodyTarget::SetVariable(char*, char*) {}

// 80196A54
void CASBodyTarget::SetVariableSubnav(int item, int option) {
  if (!option) return;
  if (item == 1) HandleBodyTypeSubnav(option);
  else if (item == 0) HandleSkinColorSubnav(option);
  else if (item == 4) HandleHairSubnav(option);
}

// 80196AB8
void CASBodyTarget::GetLocalizableSubnav(int item, unsigned short* text) {
  const char* row = nullptr;
  switch (item) {
    case 0: row = "cas_personal_skin"; break;
    case 1: row = "cas_select_body"; break;
    case 2: row = "cas_bodymorph"; break;
    case 3: row = "cas_headmorph"; break;
    case 4: row = "cas_head_hair"; break;
    case 5: row = UIDB::UIDBGetInt(const_cast<char*>("CAS_gender")) == 1 ? "cas_body_moustache" : "cas_body_makeup"; break;
    case 6: row = "cas_tattoo"; break;
    case 7: row = "back"; break;
    default: return;
  }
  CopyText(text, Text(row));
}

// 80196CA8
// Right (1): the next hair; else the previous.
void CASBodyTarget::HandleHairSubnav(int option) { PostChange(m_mediator, 0x12, option == 1 ? 0 : 1); }

// 80196D74
void CASBodyTarget::HandleBodyTypeSubnav(int option) {
  if (option == 1) PostChange(m_mediator, 0x29, 0);
  else if (option == -1) PostChange(m_mediator, 0x29, 1);
}

// 80196E48
void CASBodyTarget::HandleSkinColorSubnav(int option) {
  if (option == 1) PostChange(m_mediator, 0x2d, 0);
  else if (option == -1) PostChange(m_mediator, 0x2d, 1);
}

// 80196F1C
char* CASBodyTarget::GetVariable(char*) { return MallocString(""); }

// ===================================================================================================
// CASMiscTarget

// 8019BC1C
CASMiscTarget::CASMiscTarget() { AttachTables("CASMiscTarget"); }
// 8019C054
CASMiscTarget::~CASMiscTarget() { UnInstall(this); }
// 8019C1B0
void CASMiscTarget::SetVariable(char*, char*) {}
// 8019C1B4
char* CASMiscTarget::GetVariable(char*) { return MallocString(""); }

// 8019C1E0
void CASMiscTarget::GetLocalizableSubnav(UIScreenID screen, int item, unsigned short* text) {
  const bool male = UIDB::UIDBGetInt(const_cast<char*>("CAS_gender")) == 1;
  const char* row = nullptr;
  if (screen == 4) {
    static const char* const kRows[5] = {"cas_body", "cas_title_fashion", "cas_title_aspirations", "cas_title_personality",
                                         "cas_title_name"};
    if (item >= 0 && item < 5) row = kRows[item];
    else if (item == 5) {
      const int mode = m_mediator ? m_mediator->m_mode : 0;
      row = mode == 4 ? "cas_return_to_car" : "cas_enter_game";
    }
  } else if (screen == 0xb) {
    static const char* const kRows[5] = {"cas_acc_arm", "cas_acc_waist", "cas_body_jewelry", "cas_tattoo", "back"};
    if (item >= 0 && item < 5) row = kRows[item];
  } else if (screen == 0xc) {
    static const char* const kRows[4] = {"cas_tops_under", "cas_tops_over", "cas_tops_jackets", "back"};
    if (item >= 0 && item < 4) row = kRows[item];
  } else if (screen == 0xd) {
    static const char* const kRows[5] = {"cas_head_hair", "cas_body_hat", "cas_body_glasses", nullptr, "back"};
    if (item == 3) row = male ? "cas_body_moustache" : "cas_body_makeup";
    else if (item >= 0 && item < 5) row = kRows[item];
  } else if (screen == 0xe) {
    if (item == 0) row = male ? "cas_body_pants" : "cas_body_pants_skirts";
    else if (item == 1) row = "cas_body_shoes";
    else if (item == 2) row = "back";
  }
  if (row) CopyText(text, Text(row));
}

// ===================================================================================================
// CASFashionTarget

// 8019ADBC
CASFashionTarget::CASFashionTarget() {
  AttachTables("CASFashionTarget");
  InstallGet(this, "CAS_cfc_getModList");
  InstallSet(this, "CAS_cfa_randomize");
  InstallLoc(this, "CAS_cfa_randName");
}
// 8019B254
CASFashionTarget::~CASFashionTarget() { UnInstall(this); }

// 8019B3B0
void CASFashionTarget::SetVariable(char* name, char*) {
  if (!std::strcmp("CAS_cfa_randomize", name)) RandomizeFashion();
}
// 8019B3F4
void CASFashionTarget::RandomizeFashion() {}
// 8019B3F8
void CASFashionTarget::HandleEventChangeFocus(CasEventChangeFocus& event) { m_partFocus = event.m_prevPartFocus; }

// 8019B404
// Screen 17 (customizing): the item's left/right steps the part's option.
void CASFashionTarget::SetVariableSubnav(UIScreenID screen, int item, int option) {
  int change;
  if (option && screen == 0x11 && ConvertPartUIFocusOptionToChangeEvent(m_partFocus, item, change))
    HandleCFCSubnav(option, change);
}

// 8019B464
// The part being customized and the item (0 torso, 1 sleeves, 2 collar, 3 style, 4 colour,
// 5 material, 6 accessory) -> the change event.
int CASFashionTarget::ConvertPartUIFocusOptionToChangeEvent(int partFocus, int item, int& change) {
  struct Map {
    int focus, item, change;
  };
  static const Map kMap[] = {
      {0x2c, 0, 4},    {0x2c, 1, 6},    {0x2c, 2, 5},    {0x2c, 5, 7},    {0x27, 3, 0x1b}, {0x27, 5, 0x1c},
      {0x13, 4, 0x14}, {0x13, 6, 0x13}, {0x26, 4, 0x14}, {0x26, 6, 0x13}, {0x28, 4, 0x1e}, {0x2b, 1, 1},
      {0x2b, 5, 2},    {0x30, 5, 0x11}, {0x2d, 0, 9},    {0x2d, 1, 0xb},  {0x2d, 2, 10},   {0x2d, 5, 0xc},
      {0x2f, 3, 0xe},  {0x2f, 5, 0xf},  {0x33, 5, 0x28}, {0x33, 6, 0x27}, {0x32, 3, 0x20}, {0x32, 5, 0x21},
      {0x34, 5, 0x25},
  };
  for (const Map& m : kMap)
    if (m.focus == partFocus && m.item == item) {
      change = m.change;
      return 1;
    }
  return 0;
}

// 8019B6EC
void CASFashionTarget::HandleCFCSubnav(int option, int change) { PostChange(m_mediator, change, option == 1 ? 0 : 1); }

// 8019B7B0
char* CASFashionTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  if (!std::strcmp("CAS_cfc_getModList", name)) {
    const unsigned short* list = UIDB::UIDBGetString(const_cast<char*>("CAS_cfcModListStr"));
    int n = 0;
    while (list && list[n]) ++n;
    if (list) UCS2ToUTF8(list, n + 1, out);
  }
  return out;
}

// 8019B82C
unsigned short* CASFashionTarget::GetLocalizable(char* name) {
  if (static_cast<unsigned char>(*name - '0') < 2) ++name;
  if (std::strcmp("CAS_cfa_randName", name)) return nullptr;
  unsigned short text[0x1000];
  CopyText(text, Text("cas_fashion_rand"));
  UIQDTarget::UIQDSubstitutionStrings(text);
  return MallocText(text);
}

// 8019B900
void CASFashionTarget::GetLocalizableSubnav(UIScreenID screen, int item, unsigned short* text) {
  const char* row = nullptr;
  if (screen == 10) {
    static const char* const kRows[4] = {"cas_head_head", "cas_body_tops", "cas_title_bottoms", "cas_title_accessories"};
    if (item >= 0 && item < 4) row = kRows[item];
    else if (item == 4) {
      const int mode = m_mediator ? m_mediator->m_mode : 0;
      row = mode == 1 ? "cas_return_to_game" : mode == 5 ? "cas_return_to_car" : "cas_done";
    }
  } else if (screen == 0x11) {
    static const char* const kRows[8] = {"cas_custo_torso", "cas_custo_sleeves", "cas_custo_collar", "cas_custo_style",
                                         "cas_custo_color", "cas_custo_material", "cas_custo_acc", "cas_custo_wear"};
    if (item >= 0 && item < 8) row = kRows[item];
  }
  if (row) CopyText(text, Text(row));
}

// ===================================================================================================
// CASGeneticsTarget

int CASGeneticsTarget::s_GenerateState = 0;

namespace {
// kParentPortraitShaderID (80500678): dad's, mum's portrait frames.
const unsigned int kParentPortraitShaderID[2] = {0x5422d29fu, 0x28d0d32fu};
const unsigned int kGrandparentShaderID[4] = {0x52c90039u, 0xd8da95afu, 0xcbc05183u, 0x41d3c415u};
}  // namespace

// 80014F78
void BGExec_CASCGEGenerateStage() { CASGeneticsTarget::HandleGenerateStageBG(); }
// 80014FB0
// GC: runs the stage on the background thread; PC: at once, so the stage is always complete.
void BGCall_CASCGEGenerateStage() { BGExec_CASCGEGenerateStage(); }

// 80196F48
CASGeneticsTarget::CASGeneticsTarget() {
  AttachTables("CGETarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  UIDrawTable::InstallEntry(0x19, [this](ERC* rc) { Draw(rc); }, m_name, this);
  InstallGet(this, "CGE_exists");
  InstallSet(this, "CGE_Init");
  m_grandparent[1] = 0;
  m_grandparent[3] = 1;
  m_grandparent[0] = 0;
  m_grandparent[2] = 1;
  m_flags = (m_flags & ~0x2au) | 0x15u;
  m_genetics = new CasGenetics;
  m_genetics->LoadGrandparentList(false);
  for (int i = 0; i < 4; ++i) m_grandparentShaders[i] = AddRefShader(kGrandparentShaderID[i]);
  for (int i = 0; i < 2; ++i) m_parentShaders[i] = AddRefShader(kParentPortraitShaderID[i]);
  SIMMY_STUB("CASGeneticsTarget: the generating sound (0x2f127d7a) and its volume", 0x80196F48);
}

// 801976C4
CASGeneticsTarget::~CASGeneticsTarget() {
  UnInstall(this);
  delete m_genetics;
  m_genetics = nullptr;
  delete m_parentModel;
  m_parentModel = nullptr;
  delete m_plumbBob;
  m_plumbBob = nullptr;
  for (ERShader*& s : m_grandparentShaders)
    if (s) s->DelRef(), s = nullptr;
  for (ERShader*& s : m_parentShaders)
    if (s) s->DelRef(), s = nullptr;
}

// 80197958
// The screen loaded: the plumb bob; without a sim yet the frames are empty.
void CASGeneticsTarget::SetVariable(char* name, char*) {
  if (std::strcmp("CGE_Init", name)) return;
  s_GenerateState = 0;
  if (!m_plumbBob) m_plumbBob = new PlumbBobModel;  // GC: 0x3C bytes
  m_plumbBob->SetModel(0x6d1f0956u);
  m_plumbBob->m_bounce = 0;
  m_plumbBob->m_spin = 1;
  float rotation = 0.0f;
  if (m_mediator && m_mediator->m_scene) m_mediator->m_scene->GetObjectPosition(1, m_plumbBob->m_pos, rotation);
  m_plumbBob->m_baseHeight = g_plumbBobParms.m_height;
  CASTarget* cas = Cas();
  if (cas && !((cas->m_flags >> (cas->m_curSim % 4)) & 1)) {
    m_male = 1;
    for (unsigned int i = 0; i < 4; ++i) ClearGrandparentTexture(i);
    ClearParentTexture(0);
    ClearParentTexture(1);
  }
  SetDoneButtonState();
  SetPlumbBobLights(m_lights, m_mediator ? m_mediator->m_player : 0, kCGEPlumbBobLightDir);
}

// 80197B4C
// The gender buttons (items 4 male, 6 female): changing the gender makes the default sim of it.
void CASGeneticsTarget::SetVariableSubnav(int item, int) {
  if (item != 4 && item != 6) return;
  const int male = item == 4;
  if (!(m_flags & kGenerateDefaultSim)) {
    m_male = male;
    return;
  }
  const int was = m_male;
  m_male = male;
  if (((male && was == 0) || (item == 6 && was == 1)) && s_GenerateState == 0) {
    char* argv[1] = {const_cast<char*>("1")};
    AptViewer_CallFunction(const_cast<char*>("cgeGenerateBegin"), nullptr, nullptr, 1, argv);
    CASTarget* cas = Cas();
    if (cas) cas->m_flags &= ~(1u << (cas->m_curSim % 4));
    if (CasSimDescriptionS2C* desc = m_mediator ? m_mediator->GetEditSimDescription() : nullptr) GetDefaultSim(m_male == 1, *desc);
    s_GenerateState = 4;
    BGCall_CASCGEGenerateStage();
  }
}

// 80197C84
char* CASGeneticsTarget::GetVariable(char* name) { return MallocString(!std::strcmp("CGE_exists", name) ? "loaded" : ""); }

// 80197CFC
void CASGeneticsTarget::GetLocalizableSubnav(int item, unsigned short* text) {
  const char* row = nullptr;
  if (item == 4) row = "cas_genetics_male";
  else if (item == 5) row = "cas_genetics_continue";
  else if (item == 6) row = "cas_genetics_female";
  else if (item >= 0 && item < 4) {
    text[0] = 0;
    return;
  } else {
    return;
  }
  CopyText(text, Text(row), 0x20);
}

// 80197DF0
// Grandparent `which` (its gender is flag bit `which`) becomes number `index`, wrapping.
int CASGeneticsTarget::SetGrandparentIndex(unsigned int which, int index) {
  if (!m_genetics) return 0;
  const auto& list = m_genetics->m_grandparents[((m_flags >> which) & 1) ? 0 : 1];
  const unsigned int count = static_cast<unsigned int>(list.size());
  if (!count) return 0;
  unsigned int i = static_cast<unsigned int>(index);
  if (index < 0) i = count - 1;
  else if (i >= count) i = 0;
  m_grandparent[which] = i;
  return 1;
}

// 80197E60
int CASGeneticsTarget::ShowGrandparentTexture(unsigned int which, int index) {
  if (which > 3) which = 3;
  if (index == -1) index = static_cast<int>(m_grandparent[which]);
  CasGenetics__Grandparent* g = m_genetics ? m_genetics->GetGrandparent(((m_flags >> which) & 1) != 0, static_cast<unsigned int>(index)) : nullptr;
  if (!g || !g->m_portrait || !m_grandparentShaders[which]) return 0;
  m_grandparentShaders[which]->ReplaceTexture(g->m_portrait, 0);
  return 1;
}

// 80197EFC
// The frame's empty picture (texture 0x4353b57).
void CASGeneticsTarget::ClearGrandparentTexture(unsigned int which) {
  if (which > 3 || !m_grandparentShaders[which]) return;
  if (ERTexture* blank = AddRefTexture(0x4353b57u)) {
    m_grandparentShaders[which]->ReplaceTexture(blank, 0);
    blank->DelRef();
  }
}

// 80197F70
// The parent's portrait rendered from its model (SimImageMaker, 128x128, 8 bits) into the texture
// "parent_dad" / "parent_mom" (made, or the loaded one reused) in the frame's shader. PC: the image is
// finished a few frames later (SimImageMaker::ServiceDeferredImages) into the same texture.
int CASGeneticsTarget::CreateParentTexture(unsigned int which) {
  if (!m_genetics || !m_parentModel || which >= 2 || !m_parentShaders[which]) return 0;
  static const char* const kParentTextureBaseName[2] = {"parent_dad", "parent_mom"};  // 80500670
  SimImageMaker maker;
  maker.SetGender(m_parents[which].m_male != 0);
  maker.OverrideImageSize(0x80, 0x80, 8);
  maker.SetPortraitModels(m_parentModel);
  ETexture* image = maker.CreateImage();
  if (!image) return 0;
  const unsigned int id = EResourceManager::CalcId(const_cast<char*>(kParentTextureBaseName[which]));
  ERTexture* res = nullptr;
  if (!_textureman.IsLoaded(id)) {
    res = new ERTexture();  // GC: 0x18 bytes from the texture manager's heap
    _textureman.AddResource(res, id);
  } else {
    res = static_cast<ERTexture*>(_textureman.AddRef(id, nullptr, 0));
  }
  if (!res) return 0;
  res->Attach(image);
  m_parentShaders[which]->ReplaceTexture(res, 0);
  res->DelRef();
  return 1;
}

// 801980E0
void CASGeneticsTarget::ClearParentTexture(unsigned int which) {
  if (which >= 2 || !m_parentShaders[which]) return;
  if (ERTexture* blank = AddRefTexture(0x4353b57u)) {
    m_parentShaders[which]->ReplaceTexture(blank, 0);
    blank->DelRef();
  }
}

// 8019815C
// The plain sim of the gender: underwear-ish top, trousers, shoes.
void CASGeneticsTarget::GetDefaultSim(bool male, CasSimDescriptionS2C& desc) {
  desc.ClearSim(false);
  desc.m_male = male ? 1 : 0;
  if (!male) {
    desc.m_innerTop = 1;
    desc.m_lowerBodyTexture = 4;
    desc.m_shoesTexture = 0;
    desc.m_innerTopTexture = 0;
    desc.m_innerOption2 = 0;
    desc.m_lowerBody = 0;
    desc.m_lowerBodyOption = 0;
    desc.m_shoes = 0;
  } else {
    desc.m_lowerBody = 0;
    desc.m_lowerBodyTexture = 4;
    desc.m_shoes = 0;
    desc.m_shoesTexture = 1;
    desc.m_innerTop = 0;
    desc.m_innerTopTexture = 0;
    desc.m_innerOption2 = 1;
    desc.m_lowerBodyOption = 0;
  }
  if (m_genetics) m_genetics->ChoosePersonalityTraits(desc);
  desc.Verify();
}

// 80198214
void CASGeneticsTarget::SetDoneButtonState() {
  CASTarget* cas = Cas();
  if (cas && ((cas->m_flags >> (cas->m_curSim % 4)) & 1)) {
    char* argv[1] = {const_cast<char*>("1")};
    AptViewer_CallFunction(const_cast<char*>("cgeEnableDone"), nullptr, nullptr, 1, argv);
  }
}

// 8019828C
// "Generate": four random grandparents (the two of a side differ), then Update blends them.
int CASGeneticsTarget::BeginGenerate() {
  if (s_GenerateState != 0) return 0;
  char* argv[1] = {const_cast<char*>("1")};
  AptViewer_CallFunction(const_cast<char*>("cgeGenerateBegin"), nullptr, nullptr, 1, argv);
  SIMMY_STUB("CASGeneticsTarget::BeginGenerate: the generating sound", 0x8019828C);
  m_flags &= ~kGenerateDefaultSim;
  SetGrandparentIndex(0, static_cast<int>(GetNextRandomNumber() % 0x1a));
  SetGrandparentIndex(1, static_cast<int>(GetNextRandomNumber() % 0x1a));
  int guard = 0;
  do {
    SetGrandparentIndex(2, static_cast<int>(GetNextRandomNumber() % 0x1a));
  } while (m_grandparent[0] == m_grandparent[2] && ++guard < 100);
  guard = 0;
  do {
    SetGrandparentIndex(3, static_cast<int>(GetNextRandomNumber() % 0x1a));
  } while (m_grandparent[1] == m_grandparent[3] && ++guard < 100);
  ClearParentTexture(0);
  ClearParentTexture(1);
  s_GenerateState = 3;
  return 1;
}

// 8019843C
// The background part of a stage: the parent models for their portraits (1, 2), or the new sim
// posted (4).
void CASGeneticsTarget::HandleGenerateStageBG() {
  CASTarget* cas = Cas();
  CASGeneticsTarget* t = cas ? cas->m_genetics : nullptr;
  if (!t || !cas->m_installed) return;
  if (s_GenerateState == 1 || s_GenerateState == 2) {
    delete t->m_parentModel;
    auto* model = new SimModel;
    model->Init(&t->m_parents[s_GenerateState == 1 ? 0 : 1], true);
    model->CreateSkin(const_cast<char*>(s_GenerateState == 1 ? "parent_dad" : "parent_mom"));
    model->SetAllModels();
    model->CompositeAllSkin();
    model->ApplySkinToModels();
    model->ApplyAllLatticeMorphing();
    model->ApplyAllMorphTargets();
    t->m_parentModel = model;
  } else if (s_GenerateState == 4) {
    CasSimDescriptionS2C* desc = t->m_mediator ? t->m_mediator->GetEditSimDescription() : nullptr;
    if (!desc) return;
    desc->m_male = t->m_male == 1;
    CasEventInitSim init;
    init.m_desc = *desc;
    t->m_mediator->PostEvent(init);
  }
}

// 80198614
// The generate stages: 3 blends the parents and the sim, 1/2 make the parents' portraits, 4 waits for
// the sim, 5 shows it and enables "done".
void CASGeneticsTarget::Update() {
  CASTarget* cas = Cas();
  if (!cas) return;
  switch (s_GenerateState) {
    case 3: {
      cas->m_flags &= ~(1u << (cas->m_curSim % 4));
      cas->DeallocateMorphResources(cas->m_curSim);
      auto gp = [&](bool male, int which) {
        CasGenetics__Grandparent* g = m_genetics ? m_genetics->GetGrandparent(male, m_grandparent[which]) : nullptr;
        return g ? g->m_desc : nullptr;
      };
      CasSimDescriptionS2C *a = gp(true, 0), *b = gp(false, 1), *c = gp(true, 2), *d = gp(false, 3);
      if (m_genetics && a && b && c && d) {
        m_genetics->BlendSimDescriptions(*a, *b, m_parents[0], true);
        m_genetics->BlendSimDescriptions(*c, *d, m_parents[1], false);
      }
      CasSimDescriptionS2C* desc = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
      if (!m_genetics || !desc) {
        s_GenerateState = 0;
      } else {
        m_genetics->BlendSimDescriptions(m_parents[0], m_parents[1], *desc, m_male == 1);
        s_GenerateState = 1;
        BGCall_CASCGEGenerateStage();
      }
      break;
    }
    case 1:
    case 2: {
      const unsigned int parent = s_GenerateState == 1 ? 0 : 1;
      CreateParentTexture(parent);
      delete m_parentModel;
      m_parentModel = nullptr;
      ShowGrandparentTexture(parent * 2, -1);
      ShowGrandparentTexture(parent * 2 + 1, -1);
      s_GenerateState = s_GenerateState == 1 ? 2 : 4;
      BGCall_CASCGEGenerateStage();
      break;
    }
    case 4: s_GenerateState = 5; break;
    case 5: {
      cas->m_flags |= 1u << (cas->m_curSim % 4);
      cas->ResetSimRotations();
      SetDoneButtonState();
      m_flags &= ~kWarnOnGenerate;
      s_GenerateState = 0;
      char* argv[1] = {const_cast<char*>("1")};
      AptViewer_CallFunction(const_cast<char*>("cgeGenerateEnd"), nullptr, nullptr, 1, argv);
      break;
    }
    default: break;
  }
}

// 801989B8
// While generating, the frames of the grandparents being picked flicker through faces.
void CASGeneticsTarget::Draw(ERC* rc) {
  if (!m_plumbBob) return;
  if (s_GenerateState == 2) {
    for (unsigned int i = 2; i < 4; ++i) ShowGrandparentTexture(i, static_cast<int>(GetNextRandomNumber() % 0x1a));
  } else if (s_GenerateState == 1 || s_GenerateState == 3) {
    for (unsigned int i = 0; i < 4; ++i) ShowGrandparentTexture(i, static_cast<int>(GetNextRandomNumber() % 0x1a));
  } else if (s_GenerateState == 4) {
    rc->Lights(&m_lights);
    m_plumbBob->Draw(rc);
  }
}

// 801A08B8
CasGenetics* CASGeneticsTarget::GetCasGenetics() { return m_genetics; }
// 801A08C0
void CASGeneticsTarget::SetWarnOnGenerate(bool warn) { m_flags = warn ? (m_flags | kWarnOnGenerate) : (m_flags & ~kWarnOnGenerate); }
// 801A08E8
bool CASGeneticsTarget::GetWarnOnGenerate() { return (m_flags >> 5) & 1; }
// 801A08F4
void CASGeneticsTarget::SetGenerateDefaultSim(bool generate) {
  m_flags = generate ? (m_flags | kGenerateDefaultSim) : (m_flags & ~kGenerateDefaultSim);
}
// 801A091C
bool CASGeneticsTarget::IsGenerating() { return s_GenerateState != 0; }

// ===================================================================================================
// CASSelectionTarget

namespace {
const struct {
  const char* name;
  unsigned int id;
} kSlotShaders[16] = {
    {"CAS_csl_itemShaderA1", 0xcece5af2u}, {"CAS_csl_itemShaderA2", 0xb9c96a64u}, {"CAS_csl_itemShaderA3", 0x20c03bdeu},
    {"CAS_csl_itemShaderA4", 0x57c70b48u}, {"CAS_csl_itemShaderB1", 0xc9a39eebu}, {"CAS_csl_itemShaderB2", 0xbea4ae7du},
    {"CAS_csl_itemShaderB3", 0x27adffc7u}, {"CAS_csl_itemShaderB4", 0x50aacf51u}, {"CAS_csl_itemShaderC1", 0xc015d2c0u},
    {"CAS_csl_itemShaderC2", 0xb712e256u}, {"CAS_csl_itemShaderC3", 0xd7d56bb3u}, {"CAS_csl_itemShaderC4", 0xa0d25b25u},
    {"CAS_csl_itemShaderD1", 0x39db0a9fu}, {"CAS_csl_itemShaderD2", 0x4edc3a09u}, {"CAS_csl_itemShaderD3", 0xd0b8afaau},
    {"CAS_csl_itemShaderD4", 0xa7bf9f3cu},
};
// The slot's shader shows `texture` (GC: the EShader's texture through its vtable).
void SetSlotTexture(ERShader* slot, ERTexture* texture) {
  if (slot && slot->GetShader() && texture) slot->ReplaceTexture(texture, 0);
}
}  // namespace

// 80198ABC
CASSelectionTarget::CASSelectionTarget() {
  AttachTables("CASSelectionTarget");
  for (const char* n : {"CAS_csl_updateScroll", "CAS_csl_select", "CAS_csl_hiliteMove", "CAS_csl_arrowState"}) InstallSet(this, n);
  InstallGet(this, "CAS_csl_getLockState");
  InstallGet(this, "CAS_csl_getRectPos");
  InstallLoc(this, "CAS_csl_pageNumStr");
  CreateSelectionShaders();
  ClearTextures();
  for (const auto& s : kSlotShaders) UI2D::Instance()->InstallEntry(const_cast<char*>(s.name), s.id, const_cast<char*>(m_name), 0);
  UI2D::Instance()->InstallEntry(const_cast<char*>("CAS_csl_tileShader"), 0x9f93ff74u, const_cast<char*>(m_name), 0);
}

// 801993DC
CASSelectionTarget::~CASSelectionTarget() {
  for (int i = 0; i < 16; ++i) {
    if (m_shaders[i]) m_shaders[i]->DelRef(), m_shaders[i] = nullptr;
    if (m_textures[i]) m_textures[i]->DelRef(), m_textures[i] = nullptr;
  }
  if (m_blankShader) m_blankShader->DelRef(), m_blankShader = nullptr;
  UnInstall(this);
}

// 801995A0
// "CAS_csl_select": the slot's style into the sim (stored first, so "back" can undo it), back to the
// part's screen; "CAS_csl_hiliteMove": the highlighted slot; the page scroll and its arrows.
void CASSelectionTarget::SetVariable(char* name, char* value) {
  if (!std::strcmp("CAS_csl_select", name)) {
    const int option = ConvertGridStringToOptionArrayIdx(value);
    if (option < 0 || option >= m_count) return;
    const unsigned char locked = (m_lockFlags[option] >> 1) & 1;
    if (locked) return;
    ClearRecentlyUnlockedBitFlag(option);
    std::strncpy(m_rectPos, value, 2);
    m_rectPos[2] = 0;
    const unsigned char index = m_indexes[option];
    int change;
    if (!ConvertCurUIFocusToChangeEvent(change) || !m_mediator) return;
    CasEventStoreSim store;
    m_mediator->PostEvent(store);
    CasEventChangeSimS2C set;
    set.m_change = change;
    set.m_mode = 2;
    set.m_value = index;
    set.m_old = locked;
    m_mediator->PostEvent(set);
    if (m_mediator->m_target) m_mediator->m_target->HandleNav(0x127);
  } else if (!std::strcmp("CAS_csl_hiliteMove", name)) {
    const int option = ConvertGridStringToOptionArrayIdx(value);
    if (option < 0 || option >= m_count) {
      m_hilite = -1;
    } else {
      std::strncpy(m_rectPos, value, 2);
      m_hilite = option;
      m_rectPos[2] = 0;
    }
  } else if (!std::strcmp("CAS_csl_updateScroll", name)) {
    if (*value == '0') ScrollPageLeft();
    else if (*value == '1') ScrollPageRight();
  } else if (!std::strcmp("CAS_csl_arrowState", name)) {
    UpdateSelectionArrowState();
  }
}

// 801997C0
int CASSelectionTarget::ConvertGridStringToOptionArrayIdx(char* grid) { return ConvertColRowToOptionIdx(grid[0], std::atoi(grid + 1)); }

// 80199808
char* CASSelectionTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  if (!std::strcmp("CAS_csl_getLockState", name)) {
    GetLockStateString(out);
  } else if (!std::strcmp("CAS_csl_getRectPos", name)) {
    std::strncpy(out, m_rectPos, 3);
    out[2] = 0;
  }
  return out;
}

// 801998A4
// Per slot: '1' locked, '2' recently unlocked, '0' else.
void CASSelectionTarget::GetLockStateString(char* out) {
  char state[0x11] = {};
  for (unsigned int i = 0; i < 16; ++i) {
    const int option = ConvertGridIdxToOptionIdx(i);
    const unsigned char flags = option >= 0 && option < m_count ? m_lockFlags[option] : 0;
    state[i] = (flags & 2) ? '1' : (flags & 4) ? '2' : '0';
  }
  std::strcpy(out, state);
}

// 80199974
// GC: also the neighbourhood's clothing unlocks (CasClothingUnlocks::ClearRecentlyUnlocked).
void CASSelectionTarget::ClearRecentlyUnlockedBitFlag(int option) {
  if (option < 0 || option >= m_count) return;
  m_lockFlags[option] &= ~4;
}

// 80199A90
unsigned short* CASSelectionTarget::GetLocalizable(char* name) {
  if (static_cast<unsigned char>(*name - '0') < 2) ++name;
  if (std::strcmp("CAS_csl_pageNumStr", name)) return nullptr;
  char page[8] = {};
  if (m_hilite == -1) std::snprintf(page, sizeof page, " ");
  else std::snprintf(page, sizeof page, "%d/%d", m_hilite + 1, m_count);
  unsigned short text[0x40];
  UTF8ToUCS2(page, static_cast<int>(std::strlen(page)) + 1, text);
  UIQDTarget::UIQDSubstitutionStrings(text);
  return MallocText(text);
}

// 80199BAC
void CASSelectionTarget::ScrollPageLeft() {
  if (m_page == 1) return;
  ClearTextures(0xc, 0xf);
  ShiftShaderTexturesRight();
  --m_page;
  UpdateSelectionArrowState();
  LoadGridTextures(0, 3);
}

// 80199C18
void CASSelectionTarget::ScrollPageRight() {
  if (m_page == m_pageCount) return;
  ClearTextures(0, 3);
  ShiftShaderTexturesLeft();
  ++m_page;
  UpdateSelectionArrowState();
  LoadGridTextures(0xc, 0xf);
}

// 80199C88
// A column to the left: each slot shows the next column's texture.
void CASSelectionTarget::ShiftShaderTexturesLeft() {
  for (int i = 0; i < 12; ++i)
    if (m_shaders[i + 4] && m_shaders[i + 4]->m_textures[0]) SetSlotTexture(m_shaders[i], m_shaders[i + 4]->m_textures[0]);
  std::memmove(&m_textures[0], &m_textures[4], 12 * sizeof m_textures[0]);
  std::memset(&m_textures[12], 0, 4 * sizeof m_textures[0]);
}

// 80199D70
void CASSelectionTarget::ShiftShaderTexturesRight() {
  for (int i = 15; i > 3; --i)
    if (m_shaders[i - 4] && m_shaders[i - 4]->m_textures[0]) SetSlotTexture(m_shaders[i], m_shaders[i - 4]->m_textures[0]);
  std::memmove(&m_textures[4], &m_textures[0], 12 * sizeof m_textures[0]);
  std::memset(&m_textures[0], 0, 4 * sizeof m_textures[0]);
}

// 80199E5C
int CASSelectionTarget::ConvertGridIdxToOptionIdx(unsigned int grid) { return static_cast<int>(grid) + m_page * 4 - 8; }
// 80199E70
int CASSelectionTarget::ConvertColRowToOptionIdx(char column, int row) { return ConvertGridIdxToOptionIdx(column * 4 - 0x105 + row); }

// 80199E9C
// Entering the screen: the part (or tattoo place) of the UI focus, its styles offered, the title,
// the first page.
void CASSelectionTarget::InitScreen() {
  if (!m_mediator || !m_mediator->m_target) return;
  m_focus = m_mediator->m_target->GetCurrentUIFocus();
  m_male = UIDB::UIDBGetInt(const_cast<char*>("CAS_gender")) == 1;
  m_page = 1;
  std::memcpy(m_rectPos, "B1", 3);
  m_count = 0;
  eTattooTextureTypeS2C type;
  eBodyPartS2C part;
  if (ConvertUiFocusToBodyTextureType(static_cast<unsigned int>(m_focus), &type)) m_count = m_mediator->GetIndexes(type, m_indexes, m_lockFlags);
  else if (ConvertUiFocusToBodyPart(static_cast<unsigned int>(m_focus), &part)) m_count = m_mediator->GetIndexes(part, m_indexes, m_lockFlags);
  m_pageCount = 1;
  if (m_count > 0) {
    const int columns = m_count >> 2, rest = m_count - columns * 4;
    m_pageCount = columns;
    if (rest > 0) m_pageCount = rest > 4 ? columns + 2 : columns + 1;
    if (m_count > 4) --m_pageCount;
  }
  SetTitleShaderAccordingToCurUIFocus(static_cast<unsigned int>(m_focus));
  SetupInitialSelectionPage();
}

// 80199FF4
void CASSelectionTarget::CreateSelectionShaders() {
  for (int i = 0; i < 16; ++i) m_shaders[i] = AddRefShader(kSlotShaders[i].id);
  m_blankShader = AddRefShader(0x2df01676u);
}

// 8019A200
// The icons of slots `first`..`last` (their options' styles); slots past the options are blank.
void CASSelectionTarget::LoadGridTextures(unsigned int first, unsigned int last) {
  if (first >= last || last >= 0x10) return;
  for (unsigned int slot = first; slot <= last; ++slot) {
    const int option = ConvertGridIdxToOptionIdx(slot);
    if (option < m_count && option >= 0) {
      unsigned int id = 0;
      eTattooTextureTypeS2C type;
      eBodyPartS2C part;
      if (ConvertUiFocusToBodyTextureType(static_cast<unsigned int>(m_focus), &type))
        id = m_mediator->GetIconTattooTextureId(type, m_indexes[option]);
      else if (ConvertUiFocusToBodyPart(static_cast<unsigned int>(m_focus), &part))
        id = m_mediator->GetIconTextureId(part, m_indexes[option]);
      if (!id) continue;
      m_textures[slot] = AddRefTexture(id);
      SetSlotTexture(m_shaders[slot], m_textures[slot]);
    } else if (m_blankShader) {
      SetSlotTexture(m_shaders[slot], m_blankShader->m_textures[0]);
    }
  }
}

// 8019A424
void CASSelectionTarget::SetupInitialSelectionPage() {
  m_page = 1;
  ClearTextures();
  if (m_count > 0) LoadGridTextures(4, m_count + 4 > 0xf ? 0xf : static_cast<unsigned int>(m_count + 4));
  UpdateSelectionArrowState();
}

// 8019A488
void CASSelectionTarget::ClearTextures() { ClearTextures(0, 0xf); }

// 8019A4B0
void CASSelectionTarget::ClearTextures(unsigned int first, unsigned int last) {
  for (unsigned int slot = first; slot <= last && slot < 16; ++slot) {
    if (m_textures[slot]) {
      m_textures[slot]->DelRef();
      m_textures[slot] = nullptr;
    }
    if (m_shaders[slot] && m_blankShader) SetSlotTexture(m_shaders[slot], m_blankShader->m_textures[0]);
  }
}

// 8019A5F4
// The movie's scroll arrows: 0 none, 1 right only, 2 left only... by page.
void CASSelectionTarget::UpdateSelectionArrowState() {
  const char* state;
  if (m_pageCount == 1) state = "0";
  else if (m_page == 1) state = "2";
  else if (m_page != m_pageCount) state = "3";
  else state = "1";
  char* argv[1] = {const_cast<char*>(state)};
  AptViewer_CallFunction(const_cast<char*>("SetScrollIndicatorState"), nullptr, nullptr, 1, argv);
}

// 8019A6B4
int CASSelectionTarget::ConvertUiFocusToBodyPart(unsigned int focus, eBodyPartS2C* part) {
  int p;
  switch (focus) {
    case 0x13:
    case 0x26: p = 2; break;
    case 0x14:
    case 0x29: p = 0x12; break;
    case 0x27: p = 0x13; break;
    case 0x28: p = 0; break;
    case 0x2b: p = 7; break;
    case 0x2c: p = 9; break;
    case 0x2d: p = 0xc; break;
    case 0x2f: p = 0xf; break;
    case 0x30: p = 0x10; break;
    case 0x32: p = 0x15; break;
    case 0x33: p = 0x16; break;
    case 0x34: p = 0x14; break;
    default: return 0;
  }
  *part = p;
  return 1;
}

// 8019A7F8
int CASSelectionTarget::ConvertUiFocusToBodyTextureType(unsigned int focus, eTattooTextureTypeS2C* type) {
  if (focus < 0x3f || focus > 0x41) return 0;
  *type = static_cast<int>(focus) - 0x3f;
  return 1;
}

// 8019A854
int CASSelectionTarget::ConvertCurUIFocusToChangeEvent(int& change) {
  switch (m_focus) {
    case 0x13:
    case 0x26: change = 0x12; return 1;
    case 0x14:
    case 0x29: change = 0x16; return 1;
    case 0x27: change = 0x1a; return 1;
    case 0x28: change = 0x1d; return 1;
    case 0x2b: change = 0; return 1;
    case 0x2c: change = 3; return 1;
    case 0x2d: change = 8; return 1;
    case 0x2f: change = 0xd; return 1;
    case 0x30: change = 0x10; return 1;
    case 0x32: change = 0x1f; return 1;
    case 0x33: change = 0x26; return 1;
    case 0x34: change = 0x24; return 1;
    case 0x3f: change = 0x17; return 1;
    case 0x40: change = 0x18; return 1;
    case 0x41: change = 0x19; return 1;
    default: return 0;
  }
}

// 8019A9D0
// The grid's title tile for the part.
int CASSelectionTarget::SetTitleShaderAccordingToCurUIFocus(unsigned int focus) {
  CasSimDescriptionS2C* desc = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  const bool male = desc && desc->m_male;
  unsigned int id;
  switch (focus) {
    case 0x13:
    case 0x26: id = 0x3a04928au; break;
    case 0x14:
    case 0x29: id = male ? 0xc6b8501bu : 0xd1bebe7au; break;
    case 0x27: id = 0xb26fce00u; break;
    case 0x28: id = 0x84750f8bu; break;
    case 0x2b: id = 0x6ba88a93u; break;
    case 0x2c: id = 0x396b2854u; break;
    case 0x2d: id = 0xfd23ad57u; break;
    case 0x2f: id = male ? 0x2eb2ec2fu : 0x91a0d8b5u; break;
    case 0x30: id = 0x46019052u; break;
    case 0x32: id = 0x0510cb1fu; break;
    case 0x33: id = 0xab9cc345u; break;
    case 0x34: id = 0xaa53dc27u; break;
    case 0x3f:
    case 0x40:
    case 0x41: id = 0xb4d908d3u; break;
    default: return 0;
  }
  if (!m_installed) return 1;
  UI2D* ui2d = UI2D::Instance();
  ui2d->UnInstallEntry(const_cast<char*>("CAS_csl_tileShader"));
  ui2d->InstallEntry(const_cast<char*>("CAS_csl_tileShader"), id, const_cast<char*>(m_name), 0);
  return 1;
}

// ===================================================================================================
// CASTattooTarget

// 8019F1EC
CASTattooTarget::CASTattooTarget() {
  AttachTables("CASTattooTarget");
  InstallSet(this, "CAS_cta_clothingChange");
}
// 8019F644
CASTattooTarget::~CASTattooTarget() { UnInstall(this); }
// 8019F7A0
void CASTattooTarget::SetVariable(char*, char*) {}

// 8019F7C8
void CASTattooTarget::GetLocalizableSubnav(int item, unsigned short* text) {
  static const char* const kRows[4] = {"cas_tattoo_head_arms", "cas_body_torso", "cas_body_legs", "back"};
  if (item >= 0 && item < 4) CopyText(text, Text(kRows[item]));
}

// 8019F8D8
// Down to underwear (keeping the hair) to show the tattoos.
void CASTattooTarget::RemoveClothesForTattoo() {
  CasSimDescriptionS2C* d = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  if (!d) return;
  m_saved = *d;
  d->ClearSim(true);
  if (!d->m_male) {
    d->m_innerTop = 8;
    d->m_innerTopTexture = 2;
    d->m_innerOption2 = 0;
    d->m_lowerBody = 7;
    d->m_shoes = 4;
  } else {
    d->m_innerTop = 3;
    d->m_innerTopTexture = 0;
    d->m_innerOption2 = 3;
    d->m_lowerBody = 8;
    d->m_shoes = 9;
  }
  d->m_innerOption1 = 0;
  d->m_lowerBodyTexture = 0;
  d->m_lowerBodyOption = 0;
  d->m_shoesTexture = 0;
  d->m_bootTop = 0;
  d->m_hair = m_saved.m_hair;
  d->m_hairStyle = m_saved.m_hairStyle;
  d->m_hairTexture = m_saved.m_hairTexture;
  CasEventResetSimDraw draw;
  draw.m_genderChanged = 0;
  m_mediator->PostEvent(draw);
}

// 8019FA24
// The outfit back, with the tattoos chosen.
void CASTattooTarget::RestoreClothesForTattoo() {
  CasSimState* state = m_mediator ? m_mediator->m_editSim : nullptr;
  CasSimDescriptionS2C* d = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  if (!state || !d) return;
  state->m_stored = m_saved;
  std::memcpy(state->m_stored.m_tattoos, d->m_tattoos, sizeof d->m_tattoos);
  CasEventResetSim reset;
  reset.m_male = d->m_male;
  m_mediator->PostEvent(reset);
}

// ===================================================================================================
// CASMorphTarget (the morphed shape itself is SimModel's lattice and morph targets, not ported)


// 8019C740
CASMorphTarget::CASMorphTarget() {
  AttachTables("CASMorphTarget");
  for (const char* n : {"CAS_morphvalueH", "CAS_morphvalueV", "CMH_is_alien"}) InstallGet(this, n);
  InstallSet(this, "CAS_setmorph1");
  InstallSet(this, "CAS_setmorph2");
}
// 8019CC34
CASMorphTarget::~CASMorphTarget() { UnInstall(this); }

// 8019CD90
// A slider (0..1) to the morph's range (body 0..2, head 0..7 or 0..3); a new value is posted.
void CASMorphTarget::SetVariable(char* name, char* value) {
  if (std::strncmp("CAS_setmorph", name, 0xc) || name[0xc] == '2') return;
  double v = std::atof(value);
  if (v < 0.0) v = 0.0;
  if (v > 1.0) v = 1.0;
  const unsigned int part = static_cast<unsigned int>(m_part);
  float range;
  if (part == 2) range = 7.0f;
  else if (part < 2 || part > 7 || part < 4) range = 2.0f;
  else range = 3.0f;
  float f = static_cast<float>(v * range);
  if (f < 0.0f) f = 0.0f;
  if (f > range) f = range;
  if (static_cast<unsigned int>(m_lastPart) != part || m_last != f) {
    CasEventMorphSimS2C morph;
    morph.m_part = m_part;
    morph.m_value = f;
    if (m_mediator) m_mediator->PostEvent(morph);
    m_last = f;
    m_lastPart = m_part;
  }
}

// 8019CF0C
void CASMorphTarget::SetVariableSubnav(UIScreenID screen, int item, int option) {
  if (option) return;
  if (screen == 6) {
    if (item == 0) m_part = 0;
    else if (item == 1) m_part = 1;
    else if (item == 2) m_part = 3;
  } else if (screen == 8) {
    static const int kParts[5] = {2, 4, 6, 5, 7};
    if (item >= 0 && item < 5) {
      m_field98 = 0;
      m_part = kParts[item];
    }
  }
}

// 8019CFC4
void CASMorphTarget::GetLocalizableSubnav(UIScreenID screen, int item, unsigned short* text) {
  const char* row = nullptr;
  if (screen == 6) {
    static const char* const kRows[4] = {"cas_body_torso", "cas_body_arms", "cas_body_legs", "cas_done"};
    if (item >= 0 && item < 4) row = kRows[item];
  } else if (screen == 8) {
    static const char* const kRows[6] = {"cas_head_head", "cas_head_eyes", "cas_head_mouth", "cas_head_nose", "cas_head_jaw",
                                         "cas_done"};
    if (item >= 0 && item < 6) row = kRows[item];
  }
  if (row) CopyText(text, Text(row));
}

// 8019D1F8
// The slider's position (the morph value over its range); whether the sim is an alien.
char* CASMorphTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  CasSimDescriptionS2C* d = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  if (!d) return out;
  const size_t len = std::strlen(name);
  if (!std::strncmp("CAS_morphvalue", name, 0xe) && len == 0xf && (name[0xe] == 'H' || name[0xe] == 'V')) {
    double v = 0.0;
    switch (m_part) {
      case 0: v = d->m_morphBody * 0.5; break;
      case 1: v = d->m_morphArms * 0.5; break;
      case 3: v = d->m_morphLegs * 0.5; break;
      case 2: v = d->m_morphHead / 7.0; break;
      case 4: v = d->m_morph108 / 3.0; break;
      case 5: v = d->m_morph10C / 3.0; break;
      case 6: v = d->m_morph110 / 3.0; break;
      case 7: v = d->m_morph114 / 3.0; break;
      default: break;
    }
    std::snprintf(out, 0x20, "%f", v);
  } else if (!std::strncmp("CMH_is_alien", name, 0xc)) {
    std::snprintf(out, 0x20, "%d", d->m_head != 0);
  }
  out[0x1f] = 0;
  return out;
}

// 8019D3AC
void CASMorphTarget::EnterMorphScreen() {
  CasSimDescriptionS2C* d = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  if (!d) return;
  m_saved = *d;
  m_entered = 1;
}

// 8019D434
// Back without "done": the sim as it was.
void CASMorphTarget::ExitMorphScreenPrev() {
  if (!m_entered) return;
  m_entered = 0;
  CasEventInitSim init;
  init.m_desc = m_saved;
  if (m_mediator) m_mediator->PostEvent(init);
}

// 8019D50C
void CASMorphTarget::ExitMorphScreenDone() { m_entered = 0; }

// ===================================================================================================
// CASPersonalTarget

unsigned int CASPersonalTarget::m_highScoreAspShaderIds[25] = {};

// 8019D518
CASPersonalTarget::CASPersonalTarget() {
  AttachTables("CASPersonalTarget");
  for (const char* n : {"CAS_cpr_neatVal", "CAS_cpr_outgoingVal", "CAS_cpr_activeVal", "CAS_cpr_playfulVal", "CAS_cpr_niceVal",
                        "CAS_cap_aspVal", "CAS_cpr_exportDone"})
    InstallGet(this, n);
  for (const char* n : {"CAS_cpr_neatVal", "CAS_cpr_outgoingVal", "CAS_cpr_activeVal", "CAS_cpr_playfulVal", "CAS_cpr_niceVal",
                        "CAS_cpr_exportDone", "CAS_cpr_animate", "CAS_cap_aspVal"})
    InstallSet(this, n);
  for (const char* n : {"CAS_cpr_signName0", "CAS_cpr_signName1", "CAS_cpr_signName2", "CAS_cpr_signName3",
                        "CAS_cpr_signName4", "CAS_cpr_signName5", "CAS_cpr_signName6", "CAS_cpr_signName7",
                        "CAS_cpr_signName8", "CAS_cpr_signName9", "CAS_cpr_signName10", "CAS_cpr_signName11",
                        "CAS_cpr_textSloppy", "CAS_cpr_textNeat", "CAS_cpr_textShy", "CAS_cpr_textOutgoing",
                        "CAS_cpr_textLazy", "CAS_cpr_textActive", "CAS_cpr_textSerious", "CAS_cpr_textPlayful",
                        "CAS_cpr_textGrouchy", "CAS_cpr_textNice", "CAS_cap_highScore", "CAS_cap_description0",
                        "CAS_cap_description1", "CAS_cap_description2", "CAS_cap_description3", "CAS_cap_description4"})
    InstallLoc(this, n);
  for (const char* n : {"CAS_cap_itemShader0", "CAS_cap_itemShader1", "CAS_cap_itemShader2", "CAS_cap_itemShader3",
                        "CAS_cap_itemShader4"})
    UI2D::Instance()->InstallEntry(const_cast<char*>(n), 0x9f93ff74u, const_cast<char*>(m_name), 0);
  CalcHighScoreAspShaderIds();
  std::memset(m_savedPers, 0, sizeof m_savedPers);
}

// 8019DFC8
CASPersonalTarget::~CASPersonalTarget() { UnInstall(this); }

// 8019E124
// The five "CAS_cpr_*Val" values are kept until the screen is done; "CAS_cpr_exportDone" shows the sign
// of the points set; "CAS_cpr_animate" plays the trait's animation; "CAS_cap_aspVal" sets the
// aspiration straight into the sim.
void CASPersonalTarget::SetVariable(char* name, char* value) {
  CasSimDescriptionS2C* d = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  const signed char v = static_cast<signed char>(std::atoi(value));
  if (!std::strcmp("CAS_cpr_neatVal", name)) {
    m_savedPers[3] = v;
  } else if (!std::strcmp("CAS_cpr_outgoingVal", name)) {
    m_savedPers[0] = v;
  } else if (!std::strcmp("CAS_cpr_activeVal", name)) {
    m_savedPers[4] = v;
  } else if (!std::strcmp("CAS_cpr_playfulVal", name)) {
    m_savedPers[2] = v;
  } else if (!std::strcmp("CAS_cpr_niceVal", name)) {
    m_savedPers[1] = v;
  } else if (!std::strcmp("CAS_cpr_animate", name)) {
    HandlePersonalityAnimation(value);
  } else if (!std::strcmp("CAS_cpr_exportDone", name)) {
    char sign[3];
    std::snprintf(sign, sizeof sign, "%d", ComputeZodiacSignFromSimDesc(m_savedPers));
    sign[2] = 0;
    char* argv[1] = {sign};
    UIObjectBase::AptViewer_CallFunction(const_cast<char*>("cprSetSign"), nullptr, nullptr, 1, argv);
  } else if (!std::strcmp("CAS_cap_aspVal", name)) {
    if (d) d->m_zodiac = static_cast<unsigned char>(v);  // +0x121 (the aspiration)
  }
}

// 8019E2BC
void CASPersonalTarget::HandlePersonalityAnimation(char* value) {
  auto* renderer = m_mediator ? static_cast<CasSimRendererDynamic*>(m_mediator->m_renderer) : nullptr;
  if (renderer) renderer->PlayPersonalityAnim(ConvertFromPersonalityTextToEnum(value));
}

// 8019E300
short CASPersonalTarget::ConvertFromPersonalityTextToEnum(char* text) {
  static const char* const kTraits[10] = {"Sloppy", "Neat",    "Shy",     "Outgoing", "Lazy",
                                          "Active", "Serious", "Playful", "Grouchy",  "Nice"};
  for (short i = 0; i < 10; ++i)
    if (!std::strcmp(kTraits[i], text)) return i;
  return 5;
}

// 8019E444
// The sim's personality values, the aspiration; "CAS_cpr_exportDone" shows the sim's sign.
char* CASPersonalTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  CasSimDescriptionS2C* d = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  if (!d) return out;
  const signed char* pers = reinterpret_cast<const signed char*>(d->m_personality);
  int index = -1;
  if (!std::strcmp("CAS_cpr_neatVal", name)) index = 3;
  else if (!std::strcmp("CAS_cpr_outgoingVal", name)) index = 0;
  else if (!std::strcmp("CAS_cpr_activeVal", name)) index = 4;
  else if (!std::strcmp("CAS_cpr_playfulVal", name)) index = 2;
  else if (!std::strcmp("CAS_cpr_niceVal", name)) index = 1;
  if (index >= 0) {
    std::snprintf(out, 0x20, "%d", static_cast<int>(pers[index]));
  } else if (!std::strcmp("CAS_cpr_exportDone", name)) {
    char sign[3];
    std::snprintf(sign, sizeof sign, "%d", ComputeZodiacSignFromSimDesc(const_cast<signed char*>(pers)));
    sign[2] = 0;
    char* argv[1] = {sign};
    UIObjectBase::AptViewer_CallFunction(const_cast<char*>("cprSetSign"), nullptr, nullptr, 1, argv);
  } else if (!std::strcmp("CAS_cap_aspVal", name)) {
    std::snprintf(out, 0x20, "%d", static_cast<int>(static_cast<signed char>(d->m_zodiac)));
  }
  out[0x1f] = 0;
  return out;
}

// 8019E5FC
// "CAS_cpr_signName<n>", "CAS_cpr_text<trait>", "CAS_cap_description<n>", "CAS_cap_highScore".
unsigned short* CASPersonalTarget::GetLocalizable(char* name) {
  unsigned short text[0x1000];
  text[0] = 0;
  if (static_cast<unsigned>(*name - '0') < 2) ++name;
  if (!std::strncmp("CAS_cpr_signName", name, 0x10)) {
    ParseSignText(static_cast<short>(std::atoi(name + 0x10)), text);
  } else if (!std::strncmp("CAS_cpr_text", name, 0xc)) {
    ParsePersonalityText(name, text);
  } else if (!std::strncmp("CAS_cap_description", name, 0x13)) {
    ParseDescriptionText(static_cast<short>(std::atoi(name + 0x13)), text);
  } else if (!std::strcmp("CAS_cap_highScore", name)) {
    CopyText(text, Text("cas_asp_high"));
  } else {
    return nullptr;
  }
  UIQDTarget::UIQDSubstitutionStrings(text);
  return MallocText(text);
}

namespace {
const unsigned short* PDAText(const char* row) {
  const unsigned short* const* s = _globals.GetPDAString(row);
  return s ? *s : nullptr;
}
}  // namespace

// 8019E77C
void CASPersonalTarget::ParseSignText(short sign, unsigned short* text) {
  static const char* const kSigns[12] = {"zodiac_aries",  "zodiac_taurus",      "zodiac_gemini",    "zodiac_cancer",
                                         "zodiac_leo",    "zodiac_virgo",       "zodiac_libra",     "zodiac_scorpio",
                                         "zodiac_sagittarius", "zodiac_capricorn", "zodiac_aquarius", "zodiac_pisces"};
  if (sign < 0 || sign >= 12) {
    text[0] = 0;
    return;
  }
  CopyText(text, PDAText(kSigns[sign]));
}

// 8019E9F4
void CASPersonalTarget::ParseDescriptionText(short aspiration, unsigned short* text) {
  static const char* const kRows[5] = {"cas_asp_romance_desc", "cas_asp_wealth_desc", "cas_asp_knowledge_desc",
                                       "cas_asp_popularity_desc", "cas_asp_creativity_desc"};
  if (aspiration < 0 || aspiration >= 5) {
    text[0] = 0;
    return;
  }
  CopyText(text, Text(kRows[aspiration]));
}

// 8019EB2C
// GC: an unknown name leaves the text as it was (empty).
void CASPersonalTarget::ParsePersonalityText(char* name, unsigned short* text) {
  static const struct {
    const char* name;
    const char* row;
  } kTraits[10] = {{"CAS_cpr_textSloppy", "personality_1sloppy"},    {"CAS_cpr_textNeat", "personality_1neat"},
                   {"CAS_cpr_textShy", "personality_2shy"},          {"CAS_cpr_textOutgoing", "personality_2outgoing"},
                   {"CAS_cpr_textLazy", "personality_3lazy"},        {"CAS_cpr_textActive", "personality_3active"},
                   {"CAS_cpr_textSerious", "personality_4serious"},  {"CAS_cpr_textPlayful", "personality_4playful"},
                   {"CAS_cpr_textGrouchy", "personality_5grouchy"},  {"CAS_cpr_textNice", "personality_5nice"}};
  for (const auto& t : kTraits)
    if (!std::strcmp(t.name, name)) {
      CopyText(text, PDAText(t.row));
      return;
    }
}

// 8019EDD8
// Highlighting aspiration `item` (option 0) shows its five high-score items.
void CASPersonalTarget::SetVariableSubnav(UIScreenID screen, int item, int option) {
  if (option != 0 || screen != 0x14 || item >= 5 || item < 0) return;
  static const char* const kItems[5] = {"CAS_cap_itemShader0", "CAS_cap_itemShader1", "CAS_cap_itemShader2",
                                        "CAS_cap_itemShader3", "CAS_cap_itemShader4"};
  if (!m_installed) return;
  for (int i = 0; i < 5; ++i)
    UI2D::Instance()->InstallEntry(const_cast<char*>(kItems[i]), m_highScoreAspShaderIds[item * 5 + i],
                                   const_cast<char*>(m_name), 0);
}

// 8019EF50
void CASPersonalTarget::GetLocalizableSubnav(UIScreenID screen, int item, unsigned short* text) {
  if (screen != 0x14) return;
  static const char* const kRows[5] = {"cas_asp_romance", "cas_asp_wealth", "cas_asp_knowledge", "cas_asp_popularity",
                                       "cas_asp_creativity"};
  if (item >= 0 && item < 5) CopyText(text, Text(kRows[item]));
}

// 8019F094
void CASPersonalTarget::CalcHighScoreAspShaderIds() {
  static const unsigned int kIds[25] = {
      0x89ef7194u, 0x2bf19486u, 0xabc26526u, 0x580fdb99u, 0x06179fa4u,  // romance
      0x666bf99eu, 0x51fc5379u, 0x15308edbu, 0xa62af7f9u, 0x6699a951u,  // wealth
      0x2908319du, 0xb494cc6eu, 0x9f873cfdu, 0xa5ac851bu, 0xd6c840f1u,  // knowledge
      0xff390a74u, 0x00245f58u, 0xde81a061u, 0x6db72cceu, 0x5821a49au,  // popularity
      0xa337a53fu, 0xccc67593u, 0xe0a03245u, 0x362d6466u, 0x1ce6c830u,  // creativity
  };
  std::memcpy(m_highScoreAspShaderIds, kIds, sizeof kIds);
}

// 8019F1DC
int CASPersonalTarget::GetSavedPersVal(signed char index) { return m_savedPers[index]; }

// ===================================================================================================
// CASRoommateTarget

// 8019FB08
CASRoommateTarget::CASRoommateTarget() {
  AttachTables("CASRoommateTarget");  // GC: drawn by CASTarget::Draw, not from the draw table
  InstallGet(this, "CAS_car_enables");
  InstallSet(this, "CAS_car_beginSimSelect");
  InstallSet(this, "CAS_car_endSimSelect");
  m_plumbBob = new PlumbBobModel;  // GC: 0x3C bytes
  m_plumbBob->SetModel(0x6d1f0956u);
  m_plumbBob->m_bounce = 1;
  m_plumbBob->m_spin = 1;
  m_plumbBob->m_pos = EVec3{0.0f, 0.0f, 0.0f};
  m_plumbBob->m_baseHeight = g_plumbBobParms.m_height;
  m_lights = ELights{};
}

// 801A0048
CASRoommateTarget::~CASRoommateTarget() {
  delete m_plumbBob;
  m_plumbBob = nullptr;
  UnInstall(this);
}

// 801A01C0
// Picking a family member (with the scene up) starts on the first one there.
void CASRoommateTarget::SetVariable(char* name, char*) {
  if (!m_mediator) return;
  if (!std::strcmp("CAS_car_beginSimSelect", name)) {
    if (m_mediator->m_scene && SelectFirstNonEmptySlot()) m_selecting = 1;
  } else if (!std::strcmp("CAS_car_endSimSelect", name)) {
    if (m_mediator->m_scene) m_selecting = 0;
  }
}

// 801A0258
// Left/right (option -1/1) to the next slot with a sim.
void CASRoommateTarget::SetVariableSubnav(int, int option) {
  if (!m_selecting || !option || !m_mediator || !m_mediator->m_scene) return;
  CASTarget* cas = Cas();
  if (!cas) return;
  int slot = m_slot;
  for (int tries = 1;; ++tries) {
    slot = (slot + option) % 4;
    if (tries > 3 || (cas->m_flags >> (slot % 4) & 1)) break;
  }
  if (!(cas->m_flags >> (slot % 4) & 1)) return;
  m_slot = slot;
}

// 801A0318
void CASRoommateTarget::GetLocalizableSubnav(int item, unsigned short* text) {
  static const char* const kRows[4] = {"cas_family_add", "cas_family_edit", "cas_family_delete", "cas_family_done"};
  if (item >= 0 && item < 4) CopyText(text, Text(kRows[item]), 0x20);
}

// 801A0430
char* CASRoommateTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  if (!std::strcmp("CAS_car_enables", name)) GetButtonStateString(out);
  return out;
}

// 801A0498
int CASRoommateTarget::SelectFirstNonEmptySlot() {
  CASTarget* cas = m_mediator ? m_mediator->m_target : nullptr;
  int slot = m_slot;
  int found = 0;
  for (int i = 0; i < 4; ++i) {
    slot = (slot + i) % 4;
    if (cas && (cas->m_flags >> (slot % 4) & 1)) {
      found = 1;
      break;
    }
  }
  m_slot = slot;
  return found;
}

// 801A0514
void CASRoommateTarget::SetButtonState() {
  char state[0x10];
  GetButtonStateString(state);
  char* argv[1] = {state};
  UIObjectBase::AptViewer_CallFunction(const_cast<char*>("carCheckEnables"), nullptr, nullptr, 1, argv);
}

// 801A055C
// Add (while a slot is free), edit, delete, done (while there is a sim): '1' enabled.
void CASRoommateTarget::GetButtonStateString(char* out) {
  if (!out) return;
  CASTarget* cas = m_mediator ? m_mediator->m_target : nullptr;
  int sims = 0;
  for (int i = 0; i < 4; ++i)
    if (cas && (cas->m_flags >> (i % 4) & 1)) ++sims;
  out[0] = sims < 4 ? '1' : '0';
  out[1] = out[2] = out[3] = sims > 0 ? '1' : '0';
  out[4] = 0;
}

// 801A0604
// While picking, the plumb bob over the picked sim's head, lit in the player's colour.
void CASRoommateTarget::Draw(ERC* rc) {
  if (!m_selecting || !m_plumbBob) return;
  if (!m_lights.m_numDir) SetPlumbBobLights(m_lights, m_mediator ? m_mediator->m_player : 0, kCARPlumbBobLightDir);
  CASTarget* cas = Cas();
  if (!cas) return;
  EVec3 pos{0.0f, 0.0f, 0.0f};
  cas->m_renderers[m_slot].GetBonePos(0x10, pos);
  m_plumbBob->m_pos = pos;
  rc->Lights(&m_lights);
  m_plumbBob->Draw(rc);
}

// 801A0934
int CASRoommateTarget::GetSelectedSlot() const { return m_slot; }
