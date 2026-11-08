// Port of the edited sim's state (CasSimState, game/cassimstate.cpp): the change events from the CAS
// screens step a part or option to its next/previous available value (only styles the sim's gender
// has models for), keep the description consistent (sleeve, torso and collar styles, boot tops, hat
// hair) and repost the change as set for the renderer, which reloads the changed parts
// (CasSimRendererDynamic::HandleEventChangeSim). Also the description and parts helpers it uses
// (game/cassimdescription.cpp, cassimparts.cpp) and the game's random numbers (game/srand.cpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------------------------------
// srand

namespace {
unsigned int srandSeed = 0;  // 804FF918
}  // namespace

// 800A64E8
unsigned int GetSRandSeed() { return srandSeed; }
// 800A64F0
void SetSRandSeed(unsigned int seed) { srandSeed = seed; }
// 800A64F8
// A linear congruential step; the result is the step's high bits (mod 2^32 as on GC).
unsigned int GetNextRandomNumber() {
#if defined(__GNUC__)
  // PC: SIMMY_DET_RANDLOG=2 logs every draw's caller (the determinism harness, platform/pc_det.cpp).
  static const bool randLog = std::getenv("SIMMY_DET_RANDLOG") && std::getenv("SIMMY_DET_RANDLOG")[0] == '2';
  if (randLog)
    std::fprintf(stderr, "[rand2] frame %d seed %u from %p\n", _framecount, srandSeed, __builtin_return_address(0));
#endif
  const unsigned int lo = srandSeed & 0xffff, hi = srandSeed >> 16;
  srandSeed = srandSeed * 0x41c64e6du + 0x3039u;
  return ((lo * 0x4e6du + 0x3039u) >> 16) + lo * 0x41c6u + hi * 0x41c64e6du;
}
// PC: the same generator with its own state, for the sound code (cTrack::OnStartPlaying, the ambient
// sound player). On GC they drew from srandSeed: whether a sound started (audio device, voices, sample
// loads, the sound director's timing) then changed the trees' random numbers, inside a simulator tick
// (m_randSeed) or outside one, so the simulation depended on the audio. Same distribution, its own
// sequence (needed for lockstep netplay; see platform/pc_det.cpp).
namespace {
unsigned int soundSeed = 0;
}  // namespace
unsigned int simmy_sound_rand() {
  const unsigned int lo = soundSeed & 0xffff, hi = soundSeed >> 16;
  soundSeed = soundSeed * 0x41c64e6du + 0x3039u;
  return ((lo * 0x4e6du + 0x3039u) >> 16) + lo * 0x41c6u + hi * 0x41c64e6du;
}

// 800A6544
int RangedRandInt(unsigned int range) { return static_cast<int>(static_cast<unsigned int>(simmy::ui_rand()) % range); }

// ---------------------------------------------------------------------------------------------------
// CasSimDescriptionS2C

// 801676F0
void CasSimDescriptionS2C::SetBodyPartIndex(eBodyPartS2C part, signed char index) {
  const int p = part;
  if (p == 0) m_glasses = index;
  else if (p == 1) m_head = index;
  else if (p <= 6) m_hair = index;
  else if (p <= 8) m_innerTop = index;
  else if (p <= 0xb) m_midTop = index;
  else if (p <= 0xe) m_outerTop = index;
  else if (p == 0xf) m_lowerBody = index;
  else if (p <= 0x11) m_shoes = index;
  else if (p == 0x12) m_facialFeature = index;
  else if (p == 0x13) m_hat = index;
  else if (p == 0x14) m_necklace = index;
  else if (p == 0x15) m_armAccessory = index;
  else if (p <= 0x18) m_belt = index;
  else if (p == 0x19) m_vestScarf = index;
}

// 801678FC
void CasSimDescriptionS2C::SetBodyPartTextureIndex(eBodyPartS2C part, signed char index) {
  const int p = part;
  if (p == 0) m_glassesTexture = index;
  else if (p == 1) return;
  else if (p <= 6) m_hairTexture = index;
  else if (p <= 8) m_innerTopTexture = index;
  else if (p <= 0xb) m_midTopTexture = index;
  else if (p <= 0xe) m_outerTopTexture = index;
  else if (p == 0xf) m_lowerBodyTexture = index;
  else if (p <= 0x11) m_shoesTexture = index;
  else if (p == 0x12) return;
  else if (p == 0x13) m_hatTexture = index;
  else if (p == 0x14) m_necklaceTexture = index;
  else if (p == 0x15) m_armAccessoryTexture = index;
  else if (p <= 0x18) m_beltTexture = index;
  else if (p == 0x19) m_vestScarfTexture = index;
}

namespace {
// The part has a model for the description's style index and options.
bool HasModel(CasSimDescriptionS2C& d, CasSimPartsS2C& parts, int part, int index) {
  return parts.GetModelID(eBodyPartS2C(part), static_cast<unsigned int>(index), d.GetBodyPartParam(eBodyPartS2C(part))) != 0;
}
}  // namespace

// 801699FC
void CasSimDescriptionS2C::SetInnerLayerTorsoStyle(CasSimPartsS2C& parts) {
  m_innerOption1 = InnerLayerHalfTorsoRequired(parts) ? 1 : 0;
}

// 80169A40
void CasSimDescriptionS2C::SwitchInnerLayerTorso(bool half) { m_innerOption1 = half ? 1 : 0; }

// 80169A5C
// The mid layer's torso style between its full (0..2) and half (3..5) versions.
void CasSimDescriptionS2C::SwitchMidLayerTorso(bool half) {
  if (!half) {
    if (m_midOption1 == 3) m_midOption1 = 0;
    else if (m_midOption1 == 4) m_midOption1 = 1;
    else if (m_midOption1 == 5) m_midOption1 = 2;
  } else {
    if (m_midOption1 == 0) m_midOption1 = 3;
    else if (m_midOption1 == 1) m_midOption1 = 4;
    else if (m_midOption1 == 2) m_midOption1 = 5;
  }
}

// 80169AF8
void CasSimDescriptionS2C::SetMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C style, CasSimPartsS2C& parts) {
  m_midOption1 = style;
  SwitchMidLayerTorso(MidLayerHalfTorsoRequired(parts));
}

// 80169B38
// An outer top over it that does not need a full torso underneath.
bool CasSimDescriptionS2C::InnerLayerHalfTorsoRequired(CasSimPartsS2C& parts) {
  if (!HasModel(*this, parts, 0xc, m_outerTop)) return false;
  return parts.OuterLayerTorsoRequiresFullTorsoMeshInMidLayer(static_cast<unsigned int>(m_outerTop)) == 0;
}

// 80169BB4
// The longest inner sleeve style the top has (3 down to 0).
bool CasSimDescriptionS2C::SwitchInnerLayerSleeveToFirstAvailableStyle(CasSimPartsS2C& parts) {
  for (int style = 3; style >= 0; --style) {
    m_innerOption2 = style;
    if (HasModel(*this, parts, 8, m_innerTop) || style == 0) break;
  }
  return true;
}

// 80169CB4
bool CasSimDescriptionS2C::MidLayerHalfTorsoRequired(CasSimPartsS2C& parts) {
  if (!HasModel(*this, parts, 0xc, m_outerTop)) return false;
  return parts.OuterLayerTorsoRequiresFullTorsoMeshInMidLayer(static_cast<unsigned int>(m_outerTop)) == 0;
}

// 80169D30
// The first torso style (full 0..2 or half 3..5) the mid top has; none: the second.
void CasSimDescriptionS2C::SwitchMidLayerTorsoToFirstAvailableStyle(CasSimPartsS2C& parts, bool half) {
  const int base = half ? 3 : 0;
  for (int i = 0; i < 3; ++i) {
    m_midOption1 = base + i;
    if (HasModel(*this, parts, 9, m_midTop)) return;
  }
  m_midOption1 = base + 1;
}

// 80169E84
void CasSimDescriptionS2C::SwitchMidLayerCollarToFirstAvailableStyle(CasSimPartsS2C& parts) {
  m_midOption2 = 0;
  if (HasModel(*this, parts, 10, m_midTop)) return;
  m_midOption2 = 1;
  if (HasModel(*this, parts, 10, m_midTop)) return;
  m_midOption2 = 0;
}

// 80169F24
void CasSimDescriptionS2C::SwitchMidLayerSleeveToFirstAvailableStyle(CasSimPartsS2C& parts) {
  for (int style = 3; style >= 1; --style) {
    m_midOption3 = style;
    if (HasModel(*this, parts, 0xb, m_midTop)) return;
  }
  m_midOption3 = 0;
}

// 80169FFC
void CasSimDescriptionS2C::SwitchOuterLayerTorsoToFirstAvailableStyle(CasSimPartsS2C& parts) {
  m_outerOption1 = 0;
  if (HasModel(*this, parts, 0xc, m_outerTop)) return;
  m_outerOption1 = 1;
  if (HasModel(*this, parts, 0xc, m_outerTop)) return;
  m_outerOption1 = 0;
}

// 8016A09C
void CasSimDescriptionS2C::SwitchOuterLayerCollarToFirstAvailableStyle(CasSimPartsS2C& parts) {
  for (int style = 0; style < 4; ++style) {
    m_outerOption2 = style;
    if (HasModel(*this, parts, 0xd, m_outerTop)) return;
  }
  m_outerOption2 = 0;
}

// 8016A1AC
void CasSimDescriptionS2C::SwitchOuterLayerSleeveToFirstAvailableStyle(CasSimPartsS2C& parts) {
  m_outerOption3 = 3;
  if (HasModel(*this, parts, 0xe, m_outerTop)) return;
  m_outerOption3 = 2;
  if (HasModel(*this, parts, 0xe, m_outerTop)) return;
  m_outerOption3 = 0;
}

// 8016A24C
void CasSimDescriptionS2C::SwitchLowerBodyToFirstAvailableStyle(CasSimPartsS2C& parts) {
  for (int style = 0; style < 4; ++style) {
    m_lowerBodyOption = style;
    if (HasModel(*this, parts, 0xf, m_lowerBody) || style == 3) break;
  }
  SetShoeBoottopDisplayedFlag(parts);
}

// 8016A35C
void CasSimDescriptionS2C::SwitchHatToFirstAvailableStyle(CasSimPartsS2C& parts) {
  for (int style = 0; style < 4; ++style) {
    m_hatOption = style;
    if (HasModel(*this, parts, 0x13, m_hat) || style == 3) break;
  }
}

// 8016A460
// The shoes have a boot top and the lower body style shows it.
int CasSimDescriptionS2C::ShouldBootTopBeDisplayed(CasSimPartsS2C& parts) {
  const int saved = m_bootTop;
  m_bootTop = 1;
  const int shown = HasModel(*this, parts, 0x11, m_shoes) && m_lowerBodyOption != 0;
  m_bootTop = saved;
  return shown;
}

// 8016A4E8
void CasSimDescriptionS2C::SetShoeBoottopDisplayedFlag(CasSimPartsS2C& parts) { m_bootTop = ShouldBootTopBeDisplayed(parts); }

// 8016A824
int CasSimDescriptionS2C::GetHairBodyPart() {
  if (m_hairUnderHat) return 3;
  switch (m_hairStyle) {
    case 1: return 4;
    case 2: return 5;
    case 3: return 6;
    default: return 2;
  }
}

// ---------------------------------------------------------------------------------------------------
// CasSimPartsS2C

// 8016AB48
// The textures (colours) of style `index`: the length of the style record's texture array.
unsigned int CasSimPartsS2C::GetNumBodyTextures(eBodyPartS2C part, unsigned int index) const {
  if (!m_row) return 0;
  unsigned int off, stride, field;
  const int p = part;
  if (p == 0) off = 0x30, stride = 0x18, field = 0x10;
  else if (p == 1) return 0;
  else if (p <= 6) off = 0x20, stride = 0x28, field = 0x18;
  else if (p <= 8) off = 0x08, stride = 0x44, field = 0x0c;
  else if (p <= 0xb) off = 0x0c, stride = 0x68, field = 0x34;
  else if (p <= 0xe) off = 0x10, stride = 100, field = 0x20;
  else if (p == 0xf) off = 0x14, stride = 0x3c, field = 0x14;
  else if (p <= 0x11) off = 0x18, stride = 0x1c, field = 0x0c;
  else if (p == 0x12) return 0;
  else if (p == 0x13) off = 0x24, stride = 0x24, field = 0x14;
  else if (p == 0x14) off = 0x38, stride = 0x18, field = 0x10;
  else if (p == 0x15) off = 0x3c, stride = 0x20, field = 0x18;
  else if (p <= 0x18) off = 0x28, stride = 0x20, field = 0x10;
  else if (p == 0x19) off = 0x2c, stride = 0x18, field = 0x10;
  else return 0;
  if (index >= ArrayLength(off)) return 0;  // PC: GC reads past the array
  const unsigned char* rec = Record(off, index, stride);
  const auto* ids = rec ? static_cast<const unsigned char*>(ERQuickdata::Ptr(rec, field)) : nullptr;
  return ids ? ERQuickdata::U32(ids - 4) : 0u;
}

// 8016AD14
unsigned int CasSimPartsS2C::GetNumTattooTextures(eTattooTextureTypeS2C type) const {
  if (!m_row || type > 2 || type < 0) return 0;
  unsigned char list[600];
  return static_cast<unsigned int>(GetTattoosForLocation(type, list));
}

// 8016D388
// The tattoos (row +4, 0x14 a record) at the type's location; their count.
int CasSimPartsS2C::GetTattoosForLocation(eTattooTextureTypeS2C type, unsigned char* out) const {
  const unsigned int count = ArrayLength(4);
  const int location = ConvertTattooTextureTypeToLocation(type);
  int n = 0;
  for (unsigned int i = 0; i < count && i < 0x100; ++i) {
    const unsigned char* rec = Record(4, i, 0x14);
    if (rec && ERQuickdata::S32(rec, 8) == location) out[n++] = static_cast<unsigned char>(i);
  }
  return n;
}

// 8016D41C
int CasSimPartsS2C::ConvertTattooTextureTypeToLocation(eTattooTextureTypeS2C type) const {
  return type == 0 ? 0 : type == 2 ? 2 : 1;
}

// 8016CAC4
// The icon of style `index` for the selection grid.
unsigned int CasSimPartsS2C::GetIconTextureId(eBodyPartS2C part, unsigned int index) const {
  if (!m_row || index >= GetNumBodyModels(part)) return 0;
  auto id = [&](unsigned int off, unsigned int stride, unsigned int field) {
    const unsigned char* rec = Record(off, index, stride);
    return rec ? ERQuickdata::U32(rec, field) : 0u;
  };
  const int p = part;
  if (p == 0) return id(0x30, 0x18, 4);
  if (p == 1) return 0;
  if (p <= 6) return id(0x20, 0x28, 0x14);
  if (p <= 8) return id(0x08, 0x44, 8);
  if (p <= 0xb) return id(0x0c, 0x68, 0x2c);
  if (p <= 0xe) return id(0x10, 100, 0x18);
  if (p == 0xf) return id(0x14, 0x3c, 0x10);
  if (p <= 0x11) return id(0x18, 0x1c, 8);
  if (p == 0x12) return id(0x00, 0x18, 4);
  if (p == 0x13) return id(0x24, 0x24, 0x10);
  if (p == 0x14) return id(0x38, 0x18, 4);
  if (p == 0x15) return id(0x3c, 0x20, 0xc);
  if (p <= 0x18) return id(0x28, 0x20, 0xc);
  if (p == 0x19) return id(0x2c, 0x18, 4);
  return 0;
}

// 8016C850
// The skin texture of tattoo `index` at the type's location (the game state's version of it).
unsigned int CasSimPartsS2C::GetTattooTextureID(eTattooTextureTypeS2C type, unsigned int index) const {
  if (!m_row) return 0;
  if (index >= GetNumTattooTextures(type) || type >= 3 || type < 0) return 0;
  unsigned char list[604];
  GetTattoosForLocation(type, list);
  const unsigned char* rec = Record(4, list[index], 0x14);
  return GetGameStateTextureID(rec ? ERQuickdata::U32(rec, 0) : 0u);
}

// 8016CC9C
unsigned int CasSimPartsS2C::GetIconTattooTextureId(eTattooTextureTypeS2C type, unsigned int index) const {
  if (!m_row || index >= GetNumTattooTextures(type) || type > 2 || type < 0) return 0;
  unsigned char list[600];
  GetTattoosForLocation(type, list);
  const unsigned char* rec = Record(4, list[index], 0x14);
  return rec ? ERQuickdata::U32(rec, 4) : 0u;
}

// 8016CD30
int CasSimPartsS2C::OuterLayerTorsoRequiresFullTorsoMeshInMidLayer(unsigned int index) const {
  if (!m_row || index >= ArrayLength(0x10)) return 0;
  const unsigned char* rec = Record(0x10, index, 100);
  return rec && ERQuickdata::U8(rec, 0x1c) != 0;
}

// 8016CED8
// The top's texture can go without sleeves (a byte per texture).
int CasSimPartsS2C::CanBeWornSleeveless(eBodyPartS2C part, unsigned int index, Sim__SimPartsS2CParam param) const {
  unsigned int off, stride, field;
  if (part == 0xb) off = 0x0c, stride = 0x68, field = 0x5c;
  else if (part == 8) off = 0x08, stride = 0x44, field = 0x2c;
  else if (part == 0xe) off = 0x10, stride = 100, field = 0x40;
  else return 0;
  if (!m_row || index >= ArrayLength(off) || param.m_texture < 0) return 0;
  const unsigned char* rec = Record(off, index, stride);
  const auto* flags = rec ? static_cast<const unsigned char*>(ERQuickdata::Ptr(rec, field)) : nullptr;
  return flags && flags[param.m_texture] != 0;
}

// 8016D078
// The unlock group of style `index` (1000: never offered in CAS).
int CasSimPartsS2C::GetUnlockGroupNum(eBodyPartS2C part, unsigned int index) const {
  if (!m_row || index >= GetNumBodyModels(part)) return 0;
  auto group = [&](unsigned int off, unsigned int stride, unsigned int field) {
    const unsigned char* rec = Record(off, index, stride);
    return rec ? static_cast<int>(ERQuickdata::F32(rec, field)) : 0;
  };
  const int p = part;
  if (p == 0) return group(0x30, 0x18, 0xc);
  if (p == 1) return group(0x1c, 0x0c, 4);
  if (p <= 6) return group(0x20, 0x28, 0x24);
  if (p <= 8) return group(0x08, 0x44, 0x34);
  if (p <= 0xb) return group(0x0c, 0x68, 0x60);
  if (p <= 0xe) return group(0x10, 100, 0x44);
  if (p == 0xf) return group(0x14, 0x3c, 0x34);
  if (p <= 0x11) return group(0x18, 0x1c, 0x14);
  if (p == 0x12) return group(0x00, 0x18, 0xc);
  if (p == 0x13) return group(0x24, 0x24, 0x20);
  if (p == 0x14) return group(0x38, 0x18, 0xc);
  if (p == 0x15) return group(0x3c, 0x20, 0x14);
  if (p <= 0x18) return group(0x28, 0x20, 0x1c);
  if (p == 0x19) return group(0x2c, 0x18, 0xc);
  return 1;
}

// 8016D294
int CasSimPartsS2C::GetUnlockGroupNum(eTattooTextureTypeS2C type, unsigned int index) const {
  if (!m_row || index >= GetNumTattooTextures(type) || type > 2 || type < 0) return 0;
  unsigned char list[600];
  if (index >= static_cast<unsigned int>(GetTattoosForLocation(type, list))) return 0;
  if (list[index] >= (ArrayLength(4) & 0xff)) return 0;
  const unsigned char* rec = Record(4, list[index], 0x14);
  return rec ? static_cast<int>(ERQuickdata::F32(rec, 0x10)) : 0;
}

namespace {
// femaleMidLayerTorsoHidesBeltMatrix 803E1403 (22 mid tops x 6 options), maleMidLayerTorsoHidesBeltMatrix
// 803E1487 (19 x 6), femaleOuterLayerTorsoHidesBeltMatrix 803E14F9 (23 outer tops x 2),
// maleOuterLayerTorsoHidesBeltMatrix 803E1527 (21 x 2).
const unsigned char kFemaleMidHidesBelt[22][6] = {
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {1, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}, {1, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
const unsigned char kMaleMidHidesBelt[19][6] = {
    {0, 0, 0, 0, 0, 0}, {1, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 1, 0}, {1, 0, 0, 1, 0, 0}, {0, 1, 0, 0, 1, 0},
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {1, 0, 1, 1, 0, 1}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
const unsigned char kFemaleOuterHidesBelt[23][2] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
const unsigned char kMaleOuterHidesBelt[21][2] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 1}, {0, 1}, {0, 0}, {0, 0}, {0, 1}, {0, 0}, {0, 0},
    {0, 1}, {0, 0}, {0, 0}, {0, 1}, {0, 0}, {0, 0}, {0, 1}, {0, 0}, {0, 0}, {0, 1}};
}  // namespace

// 8016CF78
// (GC indexes the flat table by style + index * 6, so a style outside 0..5 reads a neighbouring row.)
int CasSimPartsS2C::MidLayerTorsoHidesBelt(unsigned int index, signed char style, unsigned int belt) const {
  if (!m_male) {
    if (index < 0x16) return (&kFemaleMidHidesBelt[0][0])[style + static_cast<int>(index) * 6] != 0;
  } else if (belt != 2 && index < 0x13) {
    return (&kMaleMidHidesBelt[0][0])[style + static_cast<int>(index) * 6] != 0;
  }
  return 0;
}

// 8016CFF8
int CasSimPartsS2C::OuterLayerTorsoHidesBelt(unsigned int index, signed char style, unsigned int belt) const {
  if (!m_male) {
    if (index < 0x17) return (&kFemaleOuterHidesBelt[0][0])[style + static_cast<int>(index) * 2] != 0;
  } else if (belt != 2 && index < 0x15) {
    return (&kMaleOuterHidesBelt[0][0])[style + static_cast<int>(index) * 2] != 0;
  }
  return 0;
}

// 8016D560
// head: hair 0..3, inner torso, hat; hair pieces, hat: head (hat also hair-hat); inner torso: lower body,
// head, inner sleeve; inner sleeve: inner torso; mid torso: mid collar, mid sleeve; mid collar/sleeve:
// mid torso; outer torso: outer collar, outer sleeve; outer collar/sleeve: outer torso; lower body:
// inner torso, boot top, shoes; shoes: boot top, lower body; boot top: lower body, shoes. Unused slots
// 26 (none).
void CasSimPartsS2C::GetWeldableBodyPartConnections(eBodyPartS2C part, unsigned int* out, unsigned int count) {
  if (!count || !out) return;
  for (unsigned int i = 0; i < count; ++i) out[i] = 0x1a;
  switch (static_cast<int>(part)) {
    case 1:
      out[0] = 2, out[1] = 3, out[2] = 4, out[3] = 5, out[4] = 6, out[5] = 7, out[6] = 0x13;
      break;
    case 2: case 4: case 5: case 6: out[0] = 1; break;
    case 3: out[0] = 1, out[1] = 0x13; break;
    case 7: out[0] = 0xf, out[1] = 1, out[2] = 8; break;
    case 8: out[0] = 7; break;
    case 9: out[0] = 10, out[1] = 11; break;
    case 10: case 11: out[0] = 9; break;
    case 12: out[0] = 0xd, out[1] = 0xe; break;
    case 13: case 14: out[0] = 0xc; break;
    case 15: out[0] = 7, out[1] = 0x11, out[2] = 0x10; break;
    case 16: out[0] = 0x11, out[1] = 0xf; break;
    case 17: out[0] = 0xf, out[1] = 0x10; break;
    case 19: out[0] = 3, out[1] = 1; break;
    default: break;
  }
}

// 8016D730
// How many style options the part has: hair 4, inner torso 2 / sleeve 4, mid torso 6 / collar 2 / sleeve
// 4, outer torso 2 / collar 4 / sleeve 3, lower body 4, hat 4, arm accessory 3; the rest none.
unsigned int CasSimPartsS2C::GetNumBodyPartOptions(eBodyPartS2C part) {
  static const unsigned char kOptions[0x1a] = {0, 0, 4, 4, 4, 4, 4, 2, 4, 6, 2, 4, 2, 4, 3, 4,
                                               0, 0, 0, 4, 0, 3, 0, 0, 0, 0};
  const int p = part;
  return p >= 0 && p < 0x1a ? kOptions[p] : 0;
}

// 8016D820
// The part's name for Verify's messages (the table at 803E11F0; GC's entry 24 runs into the next
// string and entry 25 is null).
const char* CasSimPartsS2C::GetBodyPartName(eBodyPartS2C part) {
  static const char* const kNames[0x1a] = {
      "Glasses", "Head", "Hair-Top", "Hair-Bottom", "Hair-Accessory-1", "Hair-Accessory-2", "Hair-Accessory-3",
      "Inner-Layer-Torso", "Inner-Layer-Torso-Sleeve", "Mid-Layer-Torso", "Mid-Layer-Torso-Collar",
      "Mid-Layer-Torso-Sleeve", "Outer-Layer-Torso", "Outer-Layer-Torso-Collar", "Outer-Layer-Torso-Sleeve",
      "Lower-Body", "Shoes", "Shoes-Boottop", "Facial-Feature", "Hat", "Necklace-Earring", "Arm-Accessory", "Belt",
      "Belt-Music-Player", "Belt-Ear-BudsVest-Scarf", "(null)"};
  const unsigned int p = static_cast<unsigned char>(static_cast<int>(part));
  return p < 0x1a ? kNames[p] : "";  // GC: DAT_803E0700, an empty string
}

// ---------------------------------------------------------------------------------------------------
// CasSimState

// 80172D68
// The next (or previous) value of the part or option the change event names, wrapping; styles the
// part has no model for are skipped. Tattoos count "none" as one more.
int CasSimState::GetAdjacentIndex(unsigned char change, bool next) {
  const int dir = next ? 1 : -1;
  CasSimPartsS2C parts;
  parts.SetGender(m_desc.m_male != 0);
  CasSimDescriptionS2C& d = m_desc;
  int max = 0, cur = 0;
  // A part's style index or a style's texture index: 0..count-1.
  auto models = [&](int part, signed char value) {
    max = static_cast<int>(parts.GetNumBodyModels(eBodyPartS2C(part))) - 1;
    cur = value;
  };
  auto textures = [&](int part, signed char style, signed char value) {
    max = static_cast<int>(parts.GetNumBodyTextures(eBodyPartS2C(part), static_cast<unsigned int>(style))) - 1;
    cur = value;
  };
  switch (change) {
    case 0x00: models(7, d.m_innerTop); break;
    case 0x01: return GetNextAvailableSleeveStyle(eBodyPartS2C(8), d.m_innerTop, next, parts);
    case 0x02: textures(7, d.m_innerTop, d.m_innerTopTexture); break;
    case 0x03: models(9, d.m_midTop); break;
    case 0x04: return GetNextAvailableMidLayerTorsoStyle(d.m_midTop, next, parts);
    case 0x05: return GetNextAvailableMidLayerCollarStyle(d.m_midTop, parts);
    case 0x06: return GetNextAvailableSleeveStyle(eBodyPartS2C(0xb), d.m_midTop, next, parts);
    case 0x07: textures(9, d.m_midTop, d.m_midTopTexture); break;
    case 0x08: models(0xc, d.m_outerTop); break;
    case 0x09: return GetNextAvailableOuterLayerTorsoStyle(d.m_outerTop, parts);
    case 0x0a: return GetNextAvailableOuterLayerCollarStyle(d.m_outerTop, next, parts);
    case 0x0b: return GetNextAvailableSleeveStyle(eBodyPartS2C(0xe), d.m_outerTop, next, parts);
    case 0x0c: textures(0xc, d.m_outerTop, d.m_outerTopTexture); break;
    case 0x0d: models(0xf, d.m_lowerBody); break;
    case 0x0e: return GetNextAvailableLowerBodyStyle(d.m_lowerBody, next, parts);
    case 0x0f: textures(0xf, d.m_lowerBody, d.m_lowerBodyTexture); break;
    case 0x10: models(0x10, d.m_shoes); break;
    case 0x11: textures(0x10, d.m_shoes, d.m_shoesTexture); break;
    case 0x12: models(2, d.m_hair); break;
    case 0x13: return GetNextAvailableHairStyle(d.m_hair, next, parts);
    case 0x14: textures(2, d.m_hair, d.m_hairTexture); break;
    case 0x15: models(1, d.m_head); break;
    case 0x16: models(0x12, d.m_facialFeature); break;
    case 0x17:
    case 0x18:
    case 0x19:
      max = static_cast<int>(parts.GetNumTattooTextures(eTattooTextureTypeS2C(change - 0x17)));
      cur = static_cast<signed char>(d.m_tattoos[change - 0x17]);
      break;
    case 0x1a: models(0x13, d.m_hat); break;
    case 0x1b: return GetNextAvailableHatStyle(d.m_hat, next, parts);
    case 0x1c: textures(0x13, d.m_hat, d.m_hatTexture); break;
    case 0x1d: models(0, d.m_glasses); break;
    case 0x1e: textures(0, d.m_glasses, d.m_glassesTexture); break;
    case 0x1f: models(0x15, d.m_armAccessory); break;
    case 0x20: return GetNextAvailableArmAccessoryStyle(d.m_armAccessory, next, parts);
    case 0x21: textures(0x15, d.m_armAccessory, d.m_armAccessoryTexture); break;
    case 0x22: models(0x19, d.m_vestScarf); break;
    case 0x23: textures(0x19, d.m_vestScarf, d.m_vestScarfTexture); break;
    case 0x24: models(0x14, d.m_necklace); break;
    case 0x25: textures(0x14, d.m_necklace, d.m_necklaceTexture); break;
    case 0x26: models(0x16, d.m_belt); break;
    case 0x27:
      max = 1;
      cur = (d.m_musicPlayer != 0 && d.m_beltExtra != 0) ? 1 : 0;
      break;
    case 0x28: textures(0x16, d.m_belt, d.m_beltTexture); break;
    case 0x29: {
      const int v = d.m_bodyType + dir;
      return v >= 3 ? 0 : v < 0 ? 2 : v;
    }
    case 0x2d: {
      const int v = d.m_skinColor + dir;
      return v >= 0xb ? 0 : v < 0 ? 10 : v;
    }
    default: return 0;
  }
  if (max == 0) return 0;
  const int v = cur + dir;
  if (v > max) return 0;
  return v < 0 ? max : v;
}

// 8017348C
// The part or option has more than one value to choose from (the screen shows its arrows).
bool CasSimState::GetMoreThanOneOption(unsigned char change) {
  CasSimPartsS2C parts;
  parts.SetGender(m_desc.m_male != 0);
  CasSimDescriptionS2C& d = m_desc;
  auto models = [&](int part) { return parts.GetNumBodyModels(eBodyPartS2C(part)) > 1; };
  auto textures = [&](int part, signed char style) {
    return parts.GetNumBodyTextures(eBodyPartS2C(part), static_cast<unsigned int>(style)) > 1;
  };
  switch (change) {
    case 0x00: return models(7);
    case 0x01: return GetNextAvailableSleeveStyle(eBodyPartS2C(8), d.m_innerTop, true, parts) != d.m_innerOption2;
    case 0x02: return textures(7, d.m_innerTop);
    case 0x03: return models(9);
    case 0x04: return GetNextAvailableMidLayerTorsoStyle(d.m_midTop, true, parts) != d.m_midOption1;
    case 0x05: return GetNextAvailableMidLayerCollarStyle(d.m_midTop, parts) != d.m_midOption2;
    case 0x06: return GetNextAvailableSleeveStyle(eBodyPartS2C(0xb), d.m_midTop, true, parts) != d.m_midOption3;
    case 0x07: return textures(9, d.m_midTop);
    case 0x08: return models(0xc);
    case 0x09: return GetNextAvailableOuterLayerTorsoStyle(d.m_outerTop, parts) != d.m_outerOption1;
    case 0x0a: return GetNextAvailableOuterLayerCollarStyle(d.m_outerTop, true, parts) != d.m_outerOption2;
    case 0x0b: return GetNextAvailableSleeveStyle(eBodyPartS2C(0xe), d.m_outerTop, true, parts) != d.m_outerOption3;
    case 0x0c: return textures(0xc, d.m_outerTop);
    case 0x0d: return models(0xf);
    case 0x0e: return GetNextAvailableLowerBodyStyle(d.m_lowerBody, true, parts) != d.m_lowerBodyOption;
    case 0x0f: return textures(0xf, d.m_lowerBody);
    case 0x10: return models(0x10);
    case 0x11: return textures(0x10, d.m_shoes);
    case 0x12: return models(2);
    case 0x13: return GetNextAvailableHairStyle(d.m_hair, true, parts) != d.m_hairStyle;
    case 0x14: return textures(2, d.m_hair);
    case 0x15: return models(1);
    case 0x16: return models(0x12);
    case 0x17:
    case 0x18:
    case 0x19: return parts.GetNumTattooTextures(eTattooTextureTypeS2C(change - 0x17)) > 1;
    case 0x1a: return models(0x13);
    case 0x1b: return GetNextAvailableHatStyle(d.m_hat, true, parts) != d.m_hatOption;
    case 0x1c: return textures(0x13, d.m_hat);
    case 0x1d: return models(0);
    case 0x1e: return textures(0, d.m_glasses);
    case 0x1f: return models(0x15);
    case 0x20: return GetNextAvailableArmAccessoryStyle(d.m_armAccessory, true, parts) != d.m_armAccessoryOption;
    case 0x21: return textures(0x15, d.m_armAccessory);
    case 0x22: return models(0x19);
    case 0x23: return textures(0x19, d.m_vestScarf);
    case 0x24: return models(0x14);
    case 0x25: return textures(0x14, d.m_necklace);
    case 0x26: return models(0x16);
    case 0x27: {
      Sim__SimPartsS2CParam param = d.GetBodyPartParam(eBodyPartS2C(0x17));
      param.m_option1 = 1;
      param.m_option2 = 1;
      return parts.GetModelID(eBodyPartS2C(0x17), static_cast<unsigned int>(d.m_belt), param) != 0;
    }
    case 0x28: return textures(0x16, d.m_belt);
    case 0x29:
    case 0x2d: return true;
    default: return false;
  }
}

// 80173B54
// The next sleeve style (part 8 inner, 0xb mid, 0xe outer) the top has a model for; sleeveless
// (style 0) only when the top's texture allows it. Back where it started: unchanged.
int CasSimState::GetNextAvailableSleeveStyle(eBodyPartS2C part, signed char index, bool next, CasSimPartsS2C& parts) {
  Sim__SimPartsS2CParam param = m_desc.GetBodyPartParam(part);
  int& style = part == 8 ? param.m_option2 : param.m_option3;
  if (part != 8 && part != 0xb && part != 0xe) return 0;
  const int start = style;
  for (int s = GetNextSleeveStyle(part, start, next); s != start; s = GetNextSleeveStyle(part, s, next)) {
    style = s;
    if (s == 0) {
      if (parts.CanBeWornSleeveless(part, static_cast<unsigned int>(index), param)) return 0;
    } else if (parts.GetModelID(part, static_cast<unsigned int>(index), param)) {
      return s;
    }
  }
  return start;
}

// 80173E5C
// The sleeve styles in order (the outer layer has no style 1).
int CasSimState::GetNextSleeveStyle(eBodyPartS2C part, Sim__SleeveStyleS2C style, bool next) {
  if (part == 0xe) {
    static const int kPrev[4] = {2, 3, 3, 0}, kNext[4] = {3, 3, 0, 2};
    return style >= 0 && style < 4 ? (next ? kNext : kPrev)[static_cast<int>(style)] : 3;
  }
  static const int kPrev[4] = {1, 2, 3, 0}, kNext[4] = {3, 0, 1, 2};
  return style >= 0 && style < 4 ? (next ? kNext : kPrev)[static_cast<int>(style)] : 3;
}

// 80173F28
int CasSimState::GetNextAvailableMidLayerTorsoStyle(signed char index, bool next, CasSimPartsS2C& parts) {
  Sim__SimPartsS2CParam param = m_desc.GetBodyPartParam(eBodyPartS2C(9));
  const int start = param.m_option1;
  int s = GetNextMidLayerTorsoStyle(start, next);
  for (; s != start; s = GetNextMidLayerTorsoStyle(s, next)) {
    param.m_option1 = s;
    if (parts.GetModelID(eBodyPartS2C(9), static_cast<unsigned int>(index), param)) return s;
  }
  return s;
}

// 80173FF8
// Within the full (0..2) or half (3..5) torso styles.
int CasSimState::GetNextMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C style, bool next) {
  static const int kPrev[6] = {2, 0, 1, 5, 3, 4}, kNext[6] = {1, 2, 0, 4, 5, 3};
  return style >= 0 && style < 6 ? (next ? kNext : kPrev)[static_cast<int>(style)] : static_cast<int>(style);
}

namespace {
// The GetNextAvailable*Style pattern: step the option through `count` values until the part has a
// model for it; back at the start, the start.
template <class Step>
int NextAvailable(CasSimDescriptionS2C& d, CasSimPartsS2C& parts, int part, int modelPart, signed char index,
                  int Sim__SimPartsS2CParam::*option, Step step) {
  Sim__SimPartsS2CParam param = d.GetBodyPartParam(eBodyPartS2C(part));
  const int start = param.*option;
  int s = step(start);
  for (; s != start; s = step(s)) {
    param.*option = s;
    if (parts.GetModelID(eBodyPartS2C(modelPart), static_cast<unsigned int>(index), param)) return s;
  }
  return s;
}
}  // namespace

// 801740A4
int CasSimState::GetNextAvailableMidLayerCollarStyle(signed char index, CasSimPartsS2C& parts) {
  return NextAvailable(m_desc, parts, 10, 10, index, &Sim__SimPartsS2CParam::m_option2,
                       [&](int s) { return GetNextBodyPartStyle(s, 2, true); });
}

// 80174178
int CasSimState::GetNextAvailableOuterLayerTorsoStyle(signed char index, CasSimPartsS2C& parts) {
  return NextAvailable(m_desc, parts, 0xc, 0xc, index, &Sim__SimPartsS2CParam::m_option1,
                       [&](int s) { return GetNextBodyPartStyle(s, 2, true); });
}

// 8017424C
int CasSimState::GetNextAvailableOuterLayerCollarStyle(signed char index, bool next, CasSimPartsS2C& parts) {
  return NextAvailable(m_desc, parts, 0xd, 0xd, index, &Sim__SimPartsS2CParam::m_option2,
                       [&](int s) { return GetNextBodyPartStyle(s, 4, next); });
}

// 80174324
int CasSimState::GetNextAvailableLowerBodyStyle(signed char index, bool next, CasSimPartsS2C& parts) {
  return NextAvailable(m_desc, parts, 0xf, 0xf, index, &Sim__SimPartsS2CParam::m_option1,
                       [&](int s) { return GetNextBodyPartStyle(s, 4, next); });
}

// 801743FC
// The hair style picks the hair part (2, 4, 5, 6) whose model is checked.
int CasSimState::GetNextAvailableHairStyle(signed char index, bool next, CasSimPartsS2C& parts) {
  static const int kPart[4] = {2, 4, 5, 6};
  Sim__SimPartsS2CParam param = m_desc.GetBodyPartParam(eBodyPartS2C(2));
  const int start = param.m_option1;
  int s = GetNextBodyPartStyle(start, 4, next);
  for (; s != start; s = GetNextBodyPartStyle(s, 4, next)) {
    param.m_option1 = s;
    if (s >= 0 && s < 4 && parts.GetModelID(eBodyPartS2C(kPart[s]), static_cast<unsigned int>(index), param)) return s;
  }
  return s;
}

// 801745CC
int CasSimState::GetNextAvailableHatStyle(signed char index, bool next, CasSimPartsS2C& parts) {
  return NextAvailable(m_desc, parts, 0x13, 0x13, index, &Sim__SimPartsS2CParam::m_option1,
                       [&](int s) { return GetNextBodyPartStyle(s, 4, next); });
}

// 801746A4
int CasSimState::GetNextAvailableArmAccessoryStyle(signed char index, bool next, CasSimPartsS2C& parts) {
  return NextAvailable(m_desc, parts, 0x15, 0x15, index, &Sim__SimPartsS2CParam::m_option1,
                       [&](int s) { return GetNextBodyPartStyle(s, 3, next); });
}

// 8017477C
int CasSimState::GetNextBodyPartStyle(int style, int count, bool next) {
  if (style < 0 || style >= count) return style;
  style += next ? 1 : -1;
  if (style >= count) return 0;
  return style < 0 ? count - 1 : style;
}

// 8017486C
// Back to the stored description (keeping the names); the renderer redraws it, rebuilt if the
// gender differs from the one asked for.
void CasSimState::HandleEventResetSim(CasEventResetSim& event) {
  unsigned short first[32], last[32];
  std::memcpy(first, m_desc.m_firstName, sizeof first);
  std::memcpy(last, m_desc.m_lastName, sizeof last);
  const int male = m_desc.m_male;
  m_desc = m_stored;
  std::memcpy(m_desc.m_firstName, first, sizeof first);
  std::memcpy(m_desc.m_lastName, last, sizeof last);
  CasEventResetSimDraw draw;
  draw.m_genderChanged = male != event.m_male;
  if (m_mediator) m_mediator->PostEvent(draw);
}

// 80174984
void CasSimState::HandleEventStoreSim(CasEventStoreSim&) { m_stored = m_desc; }

// 801749DC
// Applies the change (next, previous or a value), fixes up what depends on it, and posts it as set
// with the old value for the renderer.
void CasSimState::HandleEventChangeSim(CasEventChangeSimS2C& event) {
  int value;
  if (event.m_mode == 0 || event.m_mode == 1) {
    value = GetAdjacentIndex(static_cast<unsigned char>(event.m_change), event.m_mode == 0) & 0xff;
  } else if (event.m_mode == 2) {
    value = event.m_value & 0xff;
  } else {
    return;
  }
  CasSimPartsS2C parts;
  parts.SetGender(m_desc.m_male != 0);
  CasSimDescriptionS2C& d = m_desc;
  const signed char v = static_cast<signed char>(value);
  int old = 0;
  auto set = [&](signed char& field) {
    old = static_cast<unsigned char>(field);
    field = v;
  };
  auto setInt = [&](int& field) {
    old = static_cast<unsigned char>(field);
    field = value;
  };
  switch (event.m_change) {
    case 0x00:
      set(d.m_innerTop);
      d.m_innerTopTexture = 0;
      d.SwitchInnerLayerSleeveToFirstAvailableStyle(parts);
      d.SwitchInnerLayerTorso(d.InnerLayerHalfTorsoRequired(parts));
      break;
    case 0x01: setInt(d.m_innerOption2); break;
    case 0x02: set(d.m_innerTopTexture); break;
    case 0x03:
      set(d.m_midTop);
      d.m_midTopTexture = 0;
      d.SwitchMidLayerTorsoToFirstAvailableStyle(parts, false);
      d.SwitchMidLayerTorso(d.MidLayerHalfTorsoRequired(parts));
      d.SwitchMidLayerCollarToFirstAvailableStyle(parts);
      d.SwitchMidLayerSleeveToFirstAvailableStyle(parts);
      d.SwitchInnerLayerTorso(d.InnerLayerHalfTorsoRequired(parts));
      break;
    case 0x04: setInt(d.m_midOption1); break;
    case 0x05: setInt(d.m_midOption2); break;
    case 0x06: setInt(d.m_midOption3); break;
    case 0x07: set(d.m_midTopTexture); break;
    case 0x08:
      set(d.m_outerTop);
      d.m_outerTopTexture = 0;
      d.SwitchOuterLayerTorsoToFirstAvailableStyle(parts);
      d.SwitchOuterLayerCollarToFirstAvailableStyle(parts);
      d.SwitchOuterLayerSleeveToFirstAvailableStyle(parts);
      d.SwitchMidLayerTorso(d.MidLayerHalfTorsoRequired(parts));
      d.SwitchInnerLayerTorso(d.InnerLayerHalfTorsoRequired(parts));
      // a coat takes the vest/scarf off
      if (HasModel(d, parts, 0xc, d.m_outerTop)) d.m_vestScarf = 0;
      break;
    case 0x09: setInt(d.m_outerOption1); break;
    case 0x0a: setInt(d.m_outerOption2); break;
    case 0x0b: setInt(d.m_outerOption3); break;
    case 0x0c: set(d.m_outerTopTexture); break;
    case 0x0d:
      set(d.m_lowerBody);
      d.m_lowerBodyTexture = 0;
      d.SwitchLowerBodyToFirstAvailableStyle(parts);
      break;
    case 0x0e:
      setInt(d.m_lowerBodyOption);
      d.SetShoeBoottopDisplayedFlag(parts);
      break;
    case 0x0f: set(d.m_lowerBodyTexture); break;
    case 0x10:
      set(d.m_shoes);
      d.m_shoesTexture = 0;
      d.SetShoeBoottopDisplayedFlag(parts);
      break;
    case 0x11: set(d.m_shoesTexture); break;
    case 0x12:
      set(d.m_hair);
      d.m_hairTexture = 0;
      d.m_hairUnderHat = 0;
      d.m_hairStyle = 0;
      if (HasModel(d, parts, 0x13, d.m_hat)) d.m_hairUnderHat = 1;
      break;
    case 0x13: setInt(d.m_hairStyle); break;
    case 0x14: set(d.m_hairTexture); break;
    case 0x15: set(d.m_head); break;
    case 0x16: set(d.m_facialFeature); break;
    case 0x17:
    case 0x18:
    case 0x19:
      old = d.m_tattoos[event.m_change - 0x17];
      d.m_tattoos[event.m_change - 0x17] = static_cast<unsigned char>(value);
      break;
    case 0x1a:
      set(d.m_hat);
      d.m_hatTexture = 0;
      d.m_hairUnderHat = 0;
      d.m_hatOption = 0;
      if (HasModel(d, parts, 0x13, d.m_hat)) d.m_hairUnderHat = 1;
      break;
    case 0x1b: setInt(d.m_hatOption); break;
    case 0x1c: set(d.m_hatTexture); break;
    case 0x1d:
      set(d.m_glasses);
      d.m_glassesTexture = 0;
      break;
    case 0x1e: set(d.m_glassesTexture); break;
    case 0x1f:
      set(d.m_armAccessory);
      d.m_armAccessoryTexture = 0;
      d.m_armAccessoryOption = 0;
      break;
    case 0x20: setInt(d.m_armAccessoryOption); break;
    case 0x21: set(d.m_armAccessoryTexture); break;
    case 0x22:
      set(d.m_vestScarf);
      d.m_vestScarfTexture = 0;
      break;
    case 0x23: set(d.m_vestScarfTexture); break;
    case 0x24:
      set(d.m_necklace);
      d.m_necklaceTexture = 0;
      break;
    case 0x25: set(d.m_necklaceTexture); break;
    case 0x26:
      set(d.m_belt);
      d.m_beltExtra = 0;
      d.m_beltTexture = 0;
      d.m_musicPlayer = 0;
      break;
    case 0x27:
      old = (d.m_musicPlayer != 0 || d.m_beltExtra != 0) ? 1 : 0;
      d.m_musicPlayer = d.m_beltExtra = value != 0 ? 1 : 0;
      break;
    case 0x28: set(d.m_beltTexture); break;
    case 0x29: set(d.m_bodyType); break;
    case 0x2d: set(d.m_skinColor); break;
    default: return;
  }
  d.Verify();
  if (event.m_mode == 0 || event.m_mode == 1) {
    CasEventChangeSimS2C set2;
    set2.m_change = event.m_change;
    set2.m_mode = 2;
    set2.m_value = value;
    set2.m_old = old;
    if (m_mediator) m_mediator->PostEvent(set2);
  }
}

// 80175180
// A morph slider's value into the description.
void CasSimState::HandleEventMorphSim(CasEventMorphSimS2C& event) {
  float* const kTargets[8] = {&m_desc.m_morphBody, &m_desc.m_morphArms, &m_desc.m_morphHead, &m_desc.m_morphLegs,
                              &m_desc.m_morph108,  &m_desc.m_morph10C,  &m_desc.m_morph110,  &m_desc.m_morph114};
  if (event.m_part >= 0 && event.m_part < 8) *kTargets[event.m_part] = event.m_value;
}

// 80175228
// GC: both names go into the first name (the last name is never set), kept as on GC.
void CasSimState::SetSimName(unsigned short* first, unsigned short* last) {
  if (first) {
    std::memcpy(m_desc.m_firstName, first, sizeof m_desc.m_firstName);
    m_desc.m_firstName[31] = 0;
  }
  if (last) {
    std::memcpy(m_desc.m_firstName, first, sizeof m_desc.m_firstName);
    m_desc.m_firstName[31] = 0;
  }
}

// ---------------------------------------------------------------------------------------------------
// CasMediator / CasSimRenderer: the parts lists for the selection grid

// 801620F4
unsigned int CasMediator::GetNumBodyModels(eBodyPartS2C part) const {
  return m_renderer ? m_renderer->GetNumBodyModels(part) : 0;
}
// 80162128
unsigned int CasMediator::GetNumTattooTextures(eTattooTextureTypeS2C type) const {
  return m_renderer ? m_renderer->GetNumTattooTextures(type) : 0;
}
// 8016215C
unsigned int CasMediator::GetIconTextureId(eBodyPartS2C part, unsigned int index) const {
  return m_renderer ? m_renderer->GetIconTextureId(part, index) : 0;
}
// 80162190
unsigned int CasMediator::GetIconTattooTextureId(eTattooTextureTypeS2C type, unsigned int index) const {
  return m_renderer ? m_renderer->GetIconTattooTextureId(type, index) : 0;
}
// 801621C4
int CasMediator::GetIndexes(eBodyPartS2C part, unsigned char* indexes, unsigned char* flags) {
  return m_renderer ? static_cast<CasSimRendererDynamic*>(m_renderer)->GetIndexes(part, indexes, flags) : 0;
}
// 801621F8
int CasMediator::GetIndexes(eTattooTextureTypeS2C type, unsigned char* indexes, unsigned char* flags) {
  return m_renderer ? static_cast<CasSimRendererDynamic*>(m_renderer)->GetIndexes(type, indexes, flags) : 0;
}

// 8016F7D0
unsigned int CasSimRenderer::GetNumBodyModels(eBodyPartS2C part) {
  return m_model.m_simParts ? m_model.m_simParts->GetNumBodyModels(part) : 0;
}
// 8016F7F4
unsigned int CasSimRenderer::GetNumTattooTextures(eTattooTextureTypeS2C type) {
  return m_model.m_simParts ? m_model.m_simParts->GetNumTattooTextures(type) : 0;
}
// 8016F818
unsigned int CasSimRenderer::GetIconTextureId(eBodyPartS2C part, unsigned int index) {
  return m_model.m_simParts ? m_model.m_simParts->GetIconTextureId(part, index) : 0;
}
// 8016F83C
unsigned int CasSimRenderer::GetIconTattooTextureId(eTattooTextureTypeS2C type, unsigned int index) {
  return m_model.m_simParts ? m_model.m_simParts->GetIconTattooTextureId(type, index) : 0;
}

namespace {
// The player family's clothing unlocks (GC: Globs::pNeighborhood->GetFamily(1)->GetClothingUnlocks()).
CasClothingUnlocks* StoryFamilyClothingUnlocks() {
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  auto* family = hood ? dynamic_cast<PlayerFamilyImpl*>(hood->GetFamily(1)) : nullptr;
  return family ? family->GetClothingUnlocks() : nullptr;
}
}  // namespace

// 8016FD00
// The styles CAS offers (unlock group 1000: never) and their unlock flags (2 locked, 4 new) from the
// player family's clothing unlocks for the edited sim's gender. GC: with the dev "all clothing"
// switch (DAT_80476194, set only by the dev cheat file) every style is offered, unflagged.
int CasSimRendererDynamic::GetIndexes(eBodyPartS2C part, unsigned char* indexes, unsigned char* flags) {
  const unsigned int count = GetNumBodyModels(part);
  int n = 0;
  CasClothingUnlocks* unlocks = StoryFamilyClothingUnlocks();
  CasSimDescriptionS2C* desc = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  const bool male = desc && desc->m_male != 0;
  for (unsigned int i = 0; i < count && i < 0x100; ++i) {
    if (m_model.m_simParts && m_model.m_simParts->GetUnlockGroupNum(part, i) == 1000) continue;
    indexes[n] = static_cast<unsigned char>(i);
    flags[n] = unlocks ? unlocks->GetFlags(male, part, i) : 0;  // PC: no player family, nothing locked
    ++n;
  }
  return n;
}

// 8016FE04
int CasSimRendererDynamic::GetIndexes(eTattooTextureTypeS2C type, unsigned char* indexes, unsigned char* flags) {
  const unsigned int count = GetNumTattooTextures(type);
  int n = 0;
  CasClothingUnlocks* unlocks = StoryFamilyClothingUnlocks();
  CasSimDescriptionS2C* desc = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  const bool male = desc && desc->m_male != 0;
  for (unsigned int i = 0; i < count && i < 0x100; ++i) {
    if (m_model.m_simParts && m_model.m_simParts->GetUnlockGroupNum(type, i) == 1000) continue;
    indexes[n] = static_cast<unsigned char>(i);
    flags[n] = unlocks ? unlocks->GetFlags(male, type, i) : 0;
    ++n;
  }
  return n;
}

// ---------------------------------------------------------------------------------------------------
// CasSimRendererDynamic: showing the changes

// ---------------------------------------------------------------------------------------------------
// CasSimRendererDynamic: the idles (game/cassimrendererdynamic.cpp). m_anims (the gender's
// gCas*SimAnimations) by index: 0..2 regular idles, 3 the special idle and 4, 5 its way back, 6, 7
// into and out of the pants idle (7), 8 back from the shirt idle, 9 into it, 10..12 shirt idles,
// 13 into the shirt idle, 14, 15 into and the shoe idle (15), 16 out of it, 18 the stand, 22, 23
// pants idles, 24 shirt idle, 25 shoe idle, 26..35 the personality animations (Sloppy..Nice).

// 80170154
bool CasSimRendererDynamic::IsPersonalityAnim(unsigned int anim) {
  for (int i = 26; i < 36; ++i)
    if (anim == m_anims[i]) return true;
  return false;
}
// 801701DC
bool CasSimRendererDynamic::IsShirtIdleAnim(unsigned int anim) {
  return anim == m_anims[10] || anim == m_anims[24] || anim == m_anims[11] || anim == m_anims[12];
}
// 8017021C
bool CasSimRendererDynamic::IsShoeIdleAnim(unsigned int anim) { return anim == m_anims[15] || anim == m_anims[25]; }
// 80170244
bool CasSimRendererDynamic::IsPantIdleAnim(unsigned int anim) {
  return anim == m_anims[7] || anim == m_anims[22] || anim == m_anims[23];
}
// 80170278
bool CasSimRendererDynamic::IsRegularIdleAnim(unsigned int anim) {
  return anim == m_anims[0] || anim == m_anims[1] || anim == m_anims[2];
}

// 801702AC
unsigned int CasSimRendererDynamic::SetNextRandomShirtIdle() {
  const int roll = RangedRandInt(100);
  const unsigned int anim = roll < 15 ? m_anims[11] : roll < 30 ? m_anims[12] : m_anims[10];
  SetNextAnimation(anim);
  return anim;
}

// 80170320
unsigned int CasSimRendererDynamic::SetNextRandomRegularIdle() {
  const int roll = RangedRandInt(100);
  const unsigned int anim = roll < 15 ? m_anims[1] : roll < 30 ? m_anims[2] : m_anims[0];
  SetNextAnimation(anim);
  return anim;
}

// 80170394
// The idle that finished decides the next: m_curAnim is where the sim is heading (a regular, shirt,
// pants or shoe idle), with transitions between them; a changed top, bottom or shoes may be shown off
// with their idle; a personality animation waits for a regular idle.
void CasSimRendererDynamic::SelectNextStandingAnimation() {
  const unsigned int* a = m_anims;
  const unsigned int cur = m_anim.GetTrackAnimId(eTrackFlags{1});
  const unsigned int target = m_curAnim;
  if (IsShirtIdleAnim(cur)) {
    if (!IsShirtIdleAnim(target)) SetNextAnimation(a[13]);
    else if (m_topChanged && (simmy::ui_rand() & 1) == 0) SetNextAnimation(a[24]);
    else SetNextRandomShirtIdle();
  } else if (IsPantIdleAnim(cur)) {
    if (!IsPantIdleAnim(target)) {
      SetNextAnimation(a[8]);
    } else if (m_bottomChanged && (simmy::ui_rand() & 1) == 0) {
      SetNextAnimation((simmy::ui_rand() & 1) == 0 ? a[22] : a[23]);
    } else {
      SetNextAnimation(a[7]);
    }
  } else if (IsShoeIdleAnim(cur)) {
    if (!IsShoeIdleAnim(target)) SetNextAnimation(a[16]);
    else if (!m_shoesChanged || (simmy::ui_rand() & 1) != 0) SetNextAnimation(a[15]);
    else SetNextAnimation(a[25]);
  } else if (cur == a[13] || cur == a[8] || cur == a[16] || cur == a[5] || cur == a[18] || IsRegularIdleAnim(cur)) {
    // back at the stand: on to the target idle
    if (IsRegularIdleAnim(target)) {
      if (m_playPersonality) {
        SetNextAnimation(m_personalityAnim);
        m_playPersonality = 0;
        m_curAnim = m_personalityAnim;
      } else if (m_field220 && simmy::ui_rand() % 20 == 0) {
        SetNextAnimation(a[3]);
      } else {
        m_curAnim = SetNextRandomRegularIdle();
      }
    } else if (IsShirtIdleAnim(target)) {
      SetNextAnimation(a[9]);
    } else if (target == a[7]) {
      SetNextAnimation(a[6]);
    } else if (target == a[15]) {
      SetNextAnimation(a[14]);
    }
  } else if (cur == a[3]) {
    SetNextAnimation(a[4]);
  } else if (cur == a[9]) {
    SetNextRandomShirtIdle();
  } else if (cur == a[6]) {
    SetNextAnimation(a[7]);
  } else if (cur == a[14]) {
    SetNextAnimation(a[15]);
  } else if (cur == a[4]) {
    SetNextAnimation(a[5]);
  } else {
    m_curAnim = SetNextRandomRegularIdle();
  }
  m_playPersonality = 0;
  m_topChanged = 0;
  m_bottomChanged = 0;
  m_shoesChanged = 0;
}

// 80170704
// Trait 0..9 (Sloppy..Nice): its animation plays after the current regular idle.
void CasSimRendererDynamic::PlayPersonalityAnim(short trait) {
  m_playPersonality = 1;
  if (trait >= 0 && trait <= 9) {
    m_personalityAnim = m_anims[26 + trait];
    return;
  }
  m_playPersonality = 0;
}

// 8016FAB0
// GC: the node's position from EAnimController::CalcNodeOrient (not ported); PC: the sim's position
// raised to about head height.
void CasSimRenderer::GetBonePos(unsigned int, EVec3& pos) {
  SIMMY_STUB("CasSimRenderer::GetBonePos (EAnimController::CalcNodeOrient)", 0x8016FAB0);
  pos = m_pos;
  pos.z += 1.6f;
}

// 80171D78
// A set change reloads the changed parts (model, composited skin, morphs); body type and skin
// recomposite the skin (aliens, skin 5, get the alien head and back). The face portrait of the
// developer editor is not ported.
void CasSimRendererDynamic::HandleEventChangeSim(CasEventChangeSimS2C& event) {
  if (event.m_mode != 2) return;
  const int value = event.m_value & 0xff;
  const signed char old = static_cast<signed char>(event.m_old & 0xff);
  CasSimDescriptionS2C* desc = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  if (!desc) return;
  SimModel& m = m_model;
  switch (event.m_change) {
    case 0x00:
      m.UpdateInnerLayerTorso(true, true, true);
      m.UpdateInnerLayerSleeve(true, true, true);
      m_topChanged = 1;
      break;
    case 0x01: m.UpdateInnerLayerSleeve(true, true, true); break;
    case 0x02:
      m.UpdateInnerLayerTorso(false, true, false);
      m.UpdateInnerLayerSleeve(false, true, false);
      break;
    case 0x03:
      m.UpdateMidLayerCollar(false);
      m.UpdateMidLayerTorso(true, true, true, true);
      m.UpdateMidLayerSleeve(true, true, true, true);
      m_topChanged = 1;
      break;
    case 0x04: m.UpdateMidLayerTorso(true, true, true, true); break;
    case 0x05: m.UpdateMidLayerCollar(true); break;
    case 0x06: m.UpdateMidLayerSleeve(true, true, true, true); break;
    case 0x07:
      m.UpdateMidLayerTorso(false, true, false, false);
      m.UpdateMidLayerSleeve(false, true, false, false);
      break;
    case 0x08:
      m.UpdateOuterLayerCollar(true, true);
      m.UpdateOuterLayerTorso(true, true, true, true);
      m.UpdateOuterLayerSleeve(true, true, true, true);
      m_topChanged = 1;
      break;
    case 0x09: m.UpdateOuterLayerTorso(true, true, true, true); break;
    case 0x0a: m.UpdateOuterLayerCollar(true, true); break;
    case 0x0b: m.UpdateOuterLayerSleeve(true, true, true, true); break;
    case 0x0c:
      m.UpdateOuterLayerCollar(false, true);
      m.UpdateOuterLayerTorso(false, true, false, false);
      m.UpdateOuterLayerSleeve(false, true, false, false);
      break;
    case 0x0d:
      m.UpdateLowerBody(true, true);
      m_bottomChanged = 1;
      break;
    case 0x0e: m.UpdateLowerBody(true, true); break;
    case 0x0f: m.UpdateLowerBody(false, true); break;
    case 0x10:
      m.UpdateShoes(true, true, true);
      m_shoesChanged = 1;
      break;
    case 0x11: m.UpdateShoes(false, true, false); break;
    case 0x12: m.UpdateHair(true, true, true); break;
    case 0x13: m.UpdateHair(true, false, true); break;
    case 0x14: m.UpdateHair(false, true, false); break;
    case 0x15: m.UpdateHead(true, true, true); break;
    case 0x16: m.UpdateFacialFeature(true, true, true); break;
    case 0x17:
    case 0x18:
    case 0x19: UpdateSkin(); break;
    case 0x1a: m.UpdateHat(true, true, true); break;
    case 0x1b:
      if (value <= 3) m.UpdateHat(true, false, true);
      break;
    case 0x1c: m.UpdateHat(false, true, false); break;
    case 0x1d: m.UpdateGlasses(true, true, true); break;
    case 0x1e: m.UpdateGlasses(false, true, false); break;
    case 0x1f: m.UpdateArmAccessory(true, true, true); break;
    case 0x20:
      if (value <= 2) m.UpdateArmAccessory(true, false, true);
      break;
    case 0x21: m.UpdateArmAccessory(false, true, false); break;
    case 0x22: m.UpdateVestScarf(true, true, true); break;
    case 0x23: m.UpdateVestScarf(false, true, false); break;
    case 0x24: m.UpdateNecklaceEarring(true, true, true); break;
    case 0x25: m.UpdateNecklaceEarring(false, true, false); break;
    case 0x26: m.UpdateBelt(true, true, true); break;
    case 0x27: m.UpdateBelt(true, false, true); break;
    case 0x28: m.UpdateBelt(false, true, false); break;
    case 0x29: {
      // the body type's morphs, then (to or from heavy, whose skin differs) the skin
      desc->SetBodyTypeMorphValues();
      const float values[3] = {desc->m_morphBody, desc->m_morphArms, desc->m_morphLegs};
      const int morphParts[3] = {0, 1, 3};
      for (int i = 0; i < 3; ++i) {
        CasEventMorphSimS2C morph;
        morph.m_part = morphParts[i];
        morph.m_value = values[i];
        m_mediator->PostEvent(morph);
      }
      if ((old == 2) != (desc->m_bodyType == 2)) UpdateSkin();
      break;
    }
    case 0x2d: {
      // the alien skin (5) has the alien head
      if (old == 5 && desc->m_skinColor != 5) {
        CasEventChangeSimS2C head;
        head.m_change = 0x15;
        head.m_mode = 2;
        head.m_value = 0;
        head.m_old = desc->m_head;
        m_mediator->PostEvent(head);
      } else if (old != 5 && desc->m_skinColor == 5) {
        CasEventChangeSimS2C head;
        head.m_change = 0x15;
        head.m_mode = 2;
        head.m_value = 1;
        head.m_old = desc->m_head;
        m_mediator->PostEvent(head);
      }
      UpdateSkin();
      break;
    }
    default: return;
  }
  if (m_mediator->m_target && m_mediator->m_target->m_npcEditor)
    SIMMY_STUB("CasSimRendererDynamic::HandleEventChangeSim: the NPC editor's face image", 0x80171D78);
}

// 801725DC
// A morph slider moved: body, arms and legs (0, 1, 3) and the head shape (2) set lattice weights;
// the four face regions (4..7) the weights of the head's morph targets (slots 0, 6, 3, 9 and the two
// after each). SimRenderer::MorphParts makes the new shape when the sim is drawn.
void CasSimRendererDynamic::HandleEventMorphSim(CasEventMorphSimS2C& event) {
  _pGfx->Flush();
  MorphUtilities morph(m_model.m_parts);
  const int what = event.m_part;
  const float value = event.m_value;
  if (what < 0) return;
  if (what == 0 || what == 1 || what == 3) {
    morph.ApplyMorph(what, value);
  } else if (what == 2) {
    morph.ApplyMorphHead(value);
  } else if (what <= 7) {
    static const unsigned int kRegionSlot[8] = {0, 0, 0, 0, 0, 6, 3, 9};  // parts 4, 5, 6, 7
    EIStaticModel* head = m_model.m_parts[eBodyPartS2C::kHead];
    ERModel* model = head ? head->m_model : nullptr;
    if (model) {
      const unsigned int base = kRegionSlot[what];
      for (unsigned int i = 0; i < 3; ++i) model->SetMorphTargetWeight(base + i, 0.0f);
      float w1 = 0.0f, w2 = 0.0f;
      int i1 = -1, i2 = -1;
      morph.GetFacialMorphTargetIndexAndValue(value, w1, i1, w2, i2);
      if (i1 != -1) model->SetMorphTargetWeight(static_cast<unsigned int>(i1) + base, w1);
      if (i2 != -1) model->SetMorphTargetWeight(static_cast<unsigned int>(i2) + base, w2);
    }
  } else {
    return;
  }
  if (m_mediator->m_target && m_mediator->m_target->m_npcEditor)
    SIMMY_STUB("CasSimRendererDynamic::HandleEventMorphSim: the NPC editor's face image (CreateFaceImage)", 0x801725DC);
}

// 801729AC
// The sim's parts again (its skeleton too when the gender changed); the playing animation switches
// to the gender's version.
void CasSimRendererDynamic::HandleEventResetSimDraw(CasEventResetSimDraw& event) {
  m_model.DeallocateAllModels();
  if (event.m_genderChanged) SetupSimCommon();
  m_model.SetAllModels();
  UpdateSkin();
  m_model.ApplyAllLatticeMorphing();
  m_model.ApplyAllMorphTargets();
  for (int i = 0; i < 36; ++i) {
    if (kFemaleAnimations[i] == m_curAnim || kMaleAnimations[i] == m_curAnim) {
      m_curAnim = m_anims[i];
      break;
    }
  }
}

// 80172B18
// Turning to face the camera for the back views (event focus 0x21/0x22) and back; the idle that
// fits the screen; whether the sim may play its idles.
void CasSimRendererDynamic::HandleEventChangeFocus(CasEventChangeFocus& event) {
  const int focus = event.m_eventFocus;
  const bool wasBack = static_cast<unsigned int>(m_focus - 0x21) < 2;
  if ((focus == 0x21 || focus == 0x22) && !wasBack) m_userRotation = 3.1415925f;
  if ((focus == 0x1f || focus == 0x20) && wasBack) m_userRotation = 0.0f;
  m_focus = focus;
  if (focus == 2) m_curAnim = m_anims[7];
  else if (focus == 0 || focus == 1) m_curAnim = m_anims[10];
  else if (focus == 3) m_curAnim = m_anims[15];
  else m_curAnim = m_anims[0];
  if (focus < 0x23) m_field220 = focus > 0x16 ? 0 : (focus > 0x13 || focus < 0x12) ? 1 : 0;
  else m_field220 = (focus > 0x2f || focus < 0x25) ? 1 : 0;
}
