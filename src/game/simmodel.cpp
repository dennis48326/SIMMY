// Port of game/cassimdescription.cpp, game/cassimparts.cpp, game/simmodel.cpp and
// game/simrenderer.cpp (partial): a sim's description, the parts tables, the body built from them and
// its drawing, with the composited skin (SkinCompositor, game/skincompositor.cpp) put on the parts.
// Still stubbed: body and face morphs (MorphUtilities, lattices, morph targets), welding the parts'
// shared vertices, the description's verification and streaming, tattoos, icons of the parts.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace {
// The CAS/game model switch (DAT_80476210): set, the game's "_LOD" models are used everywhere.
int s_forceLowResSims = 0;

// zodiacPersonalityValues (803E0552): the default personality points.
const unsigned char kZodiacPersonalityValues[5] = {8, 3, 3, 5, 6};

void CopyName(unsigned short* dst, const unsigned short* src) {
  for (int i = 0; i < 32; ++i) {
    dst[i] = src ? src[i] : 0;
    if (!dst[i]) {
      for (; i < 32; ++i) dst[i] = 0;
      break;
    }
  }
}
}  // namespace

// gSimPartsMapSizeMultiplier_CAS: the CAS skin maps are twice the game's (unless forced low).
short gSimPartsMapSizeMultiplier_CAS = 2;

// ---------------------------------------------------------------------------------------------------
// CasSimDescriptionS2C

// 801672FC
CasSimDescriptionS2C::CasSimDescriptionS2C() { ClearSim(false); }

// 80167330
// Every part back to style 0 and its options to their defaults; unless kept, the gender, body, morphs,
// personality and the default first and last names too.
void CasSimDescriptionS2C::ClearSim(bool keepIdentity) {
  m_magic = 0xfeed;
  m_version = 2;
  m_hairTexture = 0;
  m_outerOption3 = 3;
  m_glasses = 0;
  m_glassesTexture = 0;
  m_hair = 0;
  m_hairStyle = 0;
  m_hairUnderHat = 0;
  m_innerTop = 0;
  m_innerTopTexture = 0;
  m_innerOption1 = 0;
  m_innerOption2 = 3;
  m_midTop = 0;
  m_midTopTexture = 0;
  m_midOption1 = 0;
  m_midOption2 = 0;
  m_midOption3 = 3;
  m_outerTop = 0;
  m_outerTopTexture = 0;
  m_outerOption1 = 0;
  m_outerOption2 = 0;
  m_lowerBody = 0;
  m_lowerBodyTexture = 0;
  m_lowerBodyOption = 0;
  m_shoes = 0;
  m_shoesTexture = 0;
  m_bootTop = 0;
  m_hatTexture = 0;
  m_hat = 0;
  m_hatOption = 0;
  m_belt = 0;
  m_beltTexture = 0;
  m_musicPlayer = 0;
  m_precomposited = 0;
  m_beltExtra = 0;
  m_vestScarf = 0;
  m_vestScarfTexture = 0;
  m_armAccessory = 0;
  m_armAccessoryTexture = 0;
  m_armAccessoryOption = 0;
  m_necklace = 0;
  m_necklaceTexture = 0;
  if (keepIdentity) return;
  m_facialFeature = 0;
  m_male = 0;
  m_bodyType = 0;
  m_skinColor = 0;
  m_head = 0;
  for (unsigned char& t : m_tattoos) t = 0;
  m_morph114 = 1.0f;
  m_morphHead = 3.0f;
  m_morphBody = 1.0f;
  m_morphArms = 1.0f;
  m_morphLegs = 1.0f;
  m_morph108 = 1.0f;
  m_morph10C = 1.0f;
  m_morph110 = 1.0f;
  for (int i = 0; i < 5; ++i) m_personality[i] = kZodiacPersonalityValues[i];
  m_zodiac = 0;
  CopyName(m_firstName, EGlobal::NewUIText("default_player1_first_name"));
  m_firstName[31] = 0;
  CopyName(m_lastName, EGlobal::NewUIText("default_player1_last_name"));
  m_lastName[31] = 0;
}

// 80167540
CasSimDescriptionS2C::CasSimDescriptionS2C(bool male) {
  ClearSim(false);
  m_male = male ? 1 : 0;
  m_morph114 = 1.0f;
  m_morphHead = 3.0f;
  m_morphBody = 1.0f;
  m_morphArms = 1.0f;
  m_morphLegs = 1.0f;
  m_morph108 = 1.0f;
  m_morph10C = 1.0f;
  m_morph110 = 1.0f;
}

// 801675B0
int CasSimDescriptionS2C::GetBodyPartIndex(eBodyPartS2C part) const {
  const int p = part;
  if (p == 0) return m_glasses;
  if (p == 1) return m_head;
  if (p >= 2 && p <= 6) return m_hair;
  if (p == 7 || p == 8) return m_innerTop;
  if (p >= 9 && p <= 11) return m_midTop;
  if (p >= 12 && p <= 14) return m_outerTop;
  if (p == 15) return m_lowerBody;
  if (p == 16 || p == 17) return m_shoes;
  if (p == 18) return m_facialFeature;
  if (p == 19) return m_hat;
  if (p == 20) return m_necklace;
  if (p == 21) return m_armAccessory;
  if (p >= 22 && p <= 24) return m_belt;
  if (p == 25) return m_vestScarf;
  return 0;
}

// 801677F0
int CasSimDescriptionS2C::GetBodyPartTextureIndex(eBodyPartS2C part) const {
  const int p = part;
  if (p == 0) return m_glassesTexture;
  if (p >= 1 && p <= 6) return m_hairTexture;
  if (p == 7 || p == 8) return m_innerTopTexture;
  if (p >= 9 && p <= 11) return m_midTopTexture;
  if (p >= 12 && p <= 14) return m_outerTopTexture;
  if (p == 15) return m_lowerBodyTexture;
  if (p == 16 || p == 17) return m_shoesTexture;
  if (p == 19) return m_hatTexture;
  if (p == 20) return m_necklaceTexture;
  if (p == 21) return m_armAccessoryTexture;
  if (p >= 22 && p <= 24) return m_beltTexture;
  if (p == 25) return m_vestScarfTexture;
  return 0;
}

// 801679E4
// The part's texture and options (parts without options give only their texture; the head and the
// facial feature none).
Sim__SimPartsS2CParam CasSimDescriptionS2C::GetBodyPartParam(eBodyPartS2C part) const {
  Sim__SimPartsS2CParam r;
  const int p = part;
  if (p == 0) {
    r.m_texture = m_glassesTexture;
  } else if (p >= 2 && p <= 6) {
    r.m_texture = m_hairTexture;
    r.m_option1 = m_hairStyle;
    r.m_option2 = m_hairUnderHat;
  } else if (p == 7 || p == 8) {
    r.m_texture = m_innerTopTexture;
    r.m_option1 = m_innerOption1;
    r.m_option2 = m_innerOption2;
  } else if (p >= 9 && p <= 11) {
    r.m_texture = m_midTopTexture;
    r.m_option1 = m_midOption1;
    r.m_option2 = m_midOption2;
    r.m_option3 = m_midOption3;
  } else if (p >= 12 && p <= 14) {
    r.m_texture = m_outerTopTexture;
    r.m_option1 = m_outerOption1;
    r.m_option2 = m_outerOption2;
    r.m_option3 = m_outerOption3;
  } else if (p == 15) {
    r.m_texture = m_lowerBodyTexture;
    r.m_option1 = m_lowerBodyOption;
  } else if (p == 16 || p == 17) {
    r.m_texture = m_shoesTexture;
    r.m_option1 = m_bootTop;
  } else if (p == 19) {
    r.m_texture = m_hatTexture;
    r.m_option1 = m_hatOption;
  } else if (p == 20) {
    r.m_texture = m_necklaceTexture;
  } else if (p == 21) {
    r.m_texture = m_armAccessoryTexture;
    r.m_option1 = m_armAccessoryOption;
  } else if (p >= 22 && p <= 24) {
    r.m_texture = m_beltTexture;
    r.m_option1 = m_musicPlayer;
    r.m_option2 = m_beltExtra;
  } else if (p == 25) {
    r.m_texture = m_vestScarfTexture;
  }
  return r;
}

// 80167B7C
// A description in the older layout (no 0xFEED mark; the mark's two bytes were the first name's first
// character): only the names and the gender are kept, on a cleared sim (every part style 0, the
// default identity); the old look (25 bytes, 13 floats, 23 bytes) is read past. GC quirk kept: the
// first name's first character becomes 0xFEED (ClearSim has already put the mark back when it is
// copied).
void CasSimDescriptionS2C::ReadOldDescription(ReconBuffer* b) {
  ClearSim(false);
  const unsigned short first = m_magic;
  m_magic = 0xfeed;
  m_firstName[0] = first;
  b->Recon16(reinterpret_cast<short*>(&m_firstName[1]), 0x1f);
  b->Recon16(reinterpret_cast<short*>(m_lastName), 0x20);
  b->ReconBool(&m_male);
  signed char bytes[25];
  for (signed char& c : bytes) b->Recon8(&c, 1);
  float floats[13];
  for (int i = 0; i < 5; ++i) b->ReconFloat(&floats[i], 1);
  b->ReconFloat(&floats[5], 3);
  for (int i = 8; i < 13; ++i) b->ReconFloat(&floats[i], 1);
  signed char more[23];
  for (signed char& c : more) b->Recon8(&c, 1);
  static const bool trace = std::getenv("SIMMY_RECON_TRACE") != nullptr;  // PC dev aid
  if (trace) {
    char name[32] = {};
    for (int i = 1; i < 31 && m_firstName[i]; ++i) name[i - 1] = static_cast<char>(m_firstName[i]);
    std::printf("[desc] old-layout description read (no 0xFEED mark): first name \"?%s\", %s\n", name,
                m_male ? "male" : "female");
  }
}

// 80167F00
// The saved look (the neighbourhood's 'User' resources, the costume files): field by field, ints held
// as a byte. Descriptions without the 0xFEED mark are an older layout (ReadOldDescription). Version 2
// adds the arm accessory's option.
void CasSimDescriptionS2C::DoStream(ReconBuffer* b, int) {
  auto r8 = [b](signed char& v) { b->Recon8(&v, 1); };
  auto u8 = [b](unsigned char& v) { b->Recon8(reinterpret_cast<signed char*>(&v), 1); };
  auto byteInt = [b](int& v) {  // an int saved as its low byte (and set to it, sign-extended)
    signed char c = static_cast<signed char>(v);
    b->Recon8(&c, 1);
    v = c;
  };
  short magic = static_cast<short>(m_magic);
  b->Recon16(&magic, 1);
  m_magic = static_cast<unsigned short>(magic);
  if (m_magic != 0xfeed) {
    ReadOldDescription(b);
    return;
  }
  u8(m_version);
  b->Recon16(reinterpret_cast<short*>(m_firstName), 0x20);
  b->Recon16(reinterpret_cast<short*>(m_lastName), 0x20);
  b->ReconBool(&m_male);
  r8(m_bodyType);
  r8(m_skinColor);
  r8(m_glasses);
  r8(m_glassesTexture);
  r8(m_head);
  r8(m_hair);
  r8(m_hairTexture);
  byteInt(m_hairStyle);
  b->ReconBool(&m_hairUnderHat);
  r8(m_innerTop);
  r8(m_innerTopTexture);
  byteInt(m_innerOption1);
  byteInt(m_innerOption2);
  r8(m_midTop);
  r8(m_midTopTexture);
  byteInt(m_midOption1);
  byteInt(m_midOption2);
  byteInt(m_midOption3);
  r8(m_outerTop);
  r8(m_outerTopTexture);
  byteInt(m_outerOption1);
  byteInt(m_outerOption2);
  byteInt(m_outerOption3);
  r8(m_lowerBody);
  r8(m_lowerBodyTexture);
  byteInt(m_lowerBodyOption);
  r8(m_shoes);
  r8(m_shoesTexture);
  b->ReconBool(&m_bootTop);
  r8(m_facialFeature);
  r8(m_hat);
  r8(m_hatTexture);
  byteInt(m_hatOption);
  r8(m_belt);
  r8(m_beltTexture);
  b->ReconBool(&m_musicPlayer);
  b->ReconBool(&m_beltExtra);
  r8(m_vestScarf);
  r8(m_vestScarfTexture);
  r8(m_armAccessory);
  r8(m_armAccessoryTexture);
  r8(m_necklace);
  r8(m_necklaceTexture);
  b->ReconFloat(&m_morphBody, 1);
  b->ReconFloat(&m_morphArms, 1);
  b->ReconFloat(&m_morphLegs, 1);
  b->ReconFloat(&m_morphHead, 1);
  b->ReconFloat(&m_morph108, 1);
  b->ReconFloat(&m_morph10C, 1);
  b->ReconFloat(&m_morph110, 1);
  b->ReconFloat(&m_morph114, 1);
  b->Recon8(reinterpret_cast<signed char*>(m_tattoos), 3);
  b->ReconBool(&m_precomposited);
  b->Recon8(reinterpret_cast<signed char*>(m_personality), 5);
  u8(m_zodiac);
  int option = 0;
  if (static_cast<signed char>(m_version) >= 2) {
    option = m_armAccessoryOption;
    byteInt(option);
  }
  m_armAccessoryOption = option;
}

// 8016A518
// The body morphs of the body type (thin, fit or heavy), by gender.
void CasSimDescriptionS2C::SetBodyTypeMorphValues() {
  if (m_bodyType == 0) {
    m_morphLegs = m_morphBody = m_morphArms = 1.0f;
  } else if (m_bodyType == 1) {
    if (!m_male) {
      m_morphArms = 1.6f;
      m_morphLegs = 1.3f;
      m_morphBody = 1.6f;
    } else {
      m_morphLegs = m_morphArms = 1.0f;
      m_morphBody = 1.6f;
    }
  } else if (m_bodyType == 2) {
    if (!m_male) {
      m_morphBody = 1.3f;
      m_morphLegs = 0.5f;
      m_morphArms = 0.8f;
    } else {
      m_morphLegs = m_morphArms = 0.8f;
      m_morphBody = 0.5f;
    }
  }
}

// 8016A5F8
// The default outfit of the gender.
void CasSimDescriptionS2C::InitializeToDefaultCostume(bool male) {
  ClearSim(false);
  if (!male) {
    m_shoes = 4;
    m_male = 0;
    m_lowerBody = 7;
    m_innerTop = 4;
  } else {
    m_shoes = 9;
    m_male = 1;
    m_innerTop = 3;
    m_lowerBody = 8;
  }
}

// 80168408
// GC builds "<message>  <first name> <last name>" (low bytes of the UTF-16 names) for a debug assert that
// the release build leaves out. PC: logged.
void CasSimDescriptionS2C::AssertWithInfo(const char* message) {
  char text[0x400 + 0x48];
  std::snprintf(text, 0x400, "%s", message);
  std::size_t n = std::strlen(text);
  text[n++] = ' ';
  text[n++] = ' ';
  for (int i = 0; i < 0x20 && m_firstName[i]; ++i) text[n++] = static_cast<char>(m_firstName[i] & 0xff);
  text[n++] = ' ';
  for (int i = 0; i < 0x20 && m_lastName[i]; ++i) text[n++] = static_cast<char>(m_lastName[i] & 0xff);
  text[n] = 0;
  std::printf("[desc] Verify: %s\n", text);
}

namespace {
// GC passes the hair, hat, lower body and arm accessory options (ints) where a signed char& is wanted,
// so on the big-endian GameCube the helpers see and write the int's high byte.
signed char HighByte(int v) { return static_cast<signed char>(static_cast<unsigned int>(v) >> 24); }
void SetHighByte(int& v, signed char c) {
  v = static_cast<int>((static_cast<unsigned int>(v) & 0x00ffffffu) | (static_cast<unsigned int>(static_cast<unsigned char>(c)) << 24));
}
}  // namespace

// 80169468
// The index in range (and, unless optional, a model): else `def`.
void CasSimDescriptionS2C::VerifyBodyPartNoTexture(CasSimPartsS2C& parts, eBodyPartS2C part, signed char& index,
                                                   signed char def, bool modelOptional) {
  char text[0x40];
  if (index < 0 || parts.GetNumBodyModels(part) <= static_cast<unsigned int>(static_cast<int>(index))) {
    std::snprintf(text, sizeof text, "bad %s index %d", parts.GetBodyPartName(part), static_cast<int>(index));
  } else {
    if (modelOptional) return;
    const Sim__SimPartsS2CParam param = GetBodyPartParam(part);
    if (parts.GetModelID(part, static_cast<unsigned int>(GetBodyPartIndex(part)), param)) return;
    std::snprintf(text, sizeof text, "%s model missing", parts.GetBodyPartName(part));
  }
  AssertWithInfo(text);
  index = def;
}

// 80169578
// The index in range, a model (unless optional), a texture list, the texture index in it and a texture:
// else the defaults.
void CasSimDescriptionS2C::VerifyBodyPartWithNoOptions(CasSimPartsS2C& parts, eBodyPartS2C part, signed char& index,
                                                       signed char& texture, signed char defIndex,
                                                       signed char defTexture, bool modelOptional) {
  char text[0x40];
  if (index < 0 || parts.GetNumBodyModels(part) <= static_cast<unsigned int>(static_cast<int>(index))) {
    std::snprintf(text, sizeof text, "bad %s index %d", parts.GetBodyPartName(part), static_cast<int>(index));
  } else {
    const Sim__SimPartsS2CParam param = GetBodyPartParam(part);
    if (!parts.GetModelID(part, static_cast<unsigned int>(GetBodyPartIndex(part)), param)) {
      if (modelOptional) return;
      std::snprintf(text, sizeof text, "%s model is missing", parts.GetBodyPartName(part));
    } else {
      const int count = static_cast<int>(parts.GetNumBodyTextures(part, static_cast<unsigned int>(static_cast<int>(index))));
      if (count == 0) {
        std::snprintf(text, sizeof text, "%s texture list size is 0", parts.GetBodyPartName(part));
        AssertWithInfo(text);
        index = defIndex;
        texture = defTexture;
      } else if (texture < 0 || count <= texture) {
        std::snprintf(text, sizeof text, "bad %s texture index %d", parts.GetBodyPartName(part), static_cast<int>(texture));
        AssertWithInfo(text);
        texture = defTexture;
      }
      if (parts.GetTextureID(part, this, false)) return;
      std::snprintf(text, sizeof text, "%s texture is NULL", parts.GetBodyPartName(part));
    }
  }
  AssertWithInfo(text);
  index = defIndex;
  texture = defTexture;
}

// 8016977C
// The same with a style option (in range of GetNumBodyPartOptions, else its default).
void CasSimDescriptionS2C::VerifyBodyPartWithOneOption(CasSimPartsS2C& parts, eBodyPartS2C part, signed char& index,
                                                       signed char& texture, signed char& option,
                                                       signed char defIndex, signed char defTexture,
                                                       signed char defOption, bool modelOptional) {
  char text[0x40];
  if (index < 0 || parts.GetNumBodyModels(part) <= static_cast<unsigned int>(static_cast<int>(index))) {
    std::snprintf(text, sizeof text, "bad %s index %d", parts.GetBodyPartName(part), static_cast<int>(index));
  } else {
    if (option < 0 || parts.GetNumBodyPartOptions(part) <= static_cast<unsigned int>(static_cast<int>(option))) {
      std::snprintf(text, sizeof text, "bad %s style index %d", parts.GetBodyPartName(part), static_cast<int>(option));
      AssertWithInfo(text);
      option = defOption;
    }
    const Sim__SimPartsS2CParam param = GetBodyPartParam(part);
    if (!parts.GetModelID(part, static_cast<unsigned int>(GetBodyPartIndex(part)), param)) {
      if (modelOptional) return;
      std::snprintf(text, sizeof text, "%s model is missing", parts.GetBodyPartName(part));
    } else {
      const int count = static_cast<int>(parts.GetNumBodyTextures(part, static_cast<unsigned int>(static_cast<int>(index))));
      if (count == 0) {
        std::snprintf(text, sizeof text, "%s texture list size is 0", parts.GetBodyPartName(part));
        AssertWithInfo(text);
        index = defIndex;
        texture = defTexture;
        option = defOption;
      } else if (texture < 0 || count <= texture) {
        std::snprintf(text, sizeof text, "bad %s texture index %d", parts.GetBodyPartName(part), static_cast<int>(texture));
        AssertWithInfo(text);
        texture = defTexture;
      }
      if (parts.GetTextureID(part, this, false)) return;
      std::snprintf(text, sizeof text, "%s texture is NULL", parts.GetBodyPartName(part));
    }
  }
  AssertWithInfo(text);
  index = defIndex;
  texture = defTexture;
  option = defOption;
}

// 8016850C
// Body type (0..2) and skin (0..10); each part's index, texture and style (glasses, head, hat, hair,
// the three torso layers with their styles and the models and textures they need, lower body, shoes
// and the boot-top flag, facial feature, belt, vest/scarf, arm accessory, necklace); the hair bottom
// flag from the hat's model; the tattoos; the morphs (body 0..2, head 0..7, face 0..3); personality
// points (each 0..10, else Aries'); the aspiration (0..11); names ending within 32 characters (else
// player 1's default name).
void CasSimDescriptionS2C::Verify() {
  CasSimPartsS2C parts;
  parts.SetGender(m_male != 0);
  if (static_cast<unsigned char>(m_bodyType) > 2) {
    AssertWithInfo("bad body type");
    m_bodyType = 0;
  }
  if (static_cast<unsigned char>(m_skinColor) > 10) {
    AssertWithInfo("bad skin color");
    m_skinColor = 0;
  }
  VerifyBodyPartWithNoOptions(parts, eBodyPartS2C{0}, m_glasses, m_glassesTexture, 0, 0, true);
  VerifyBodyPartNoTexture(parts, eBodyPartS2C{1}, m_head, 0, false);
  {
    signed char option = HighByte(m_hatOption);
    VerifyBodyPartWithOneOption(parts, eBodyPartS2C{0x13}, m_hat, m_hatTexture, option, 0, 0, 0, true);
    SetHighByte(m_hatOption, option);
  }
  const int hairPart = GetHairBodyPart();
  if (!parts.GetModelID(eBodyPartS2C{0x13}, static_cast<unsigned int>(GetBodyPartIndex(eBodyPartS2C{0x13})),
                        GetBodyPartParam(eBodyPartS2C{0x13}))) {
    if (m_hairUnderHat) {
      AssertWithInfo("m_bHairBottomDisplayed incorrectly set");
      m_hairUnderHat = 0;
    }
  } else if (!m_hairUnderHat) {
    AssertWithInfo("m_bHairBottomDisplayed incorrectly set");
    m_hairUnderHat = 1;
  }
  {
    signed char option = HighByte(m_hairStyle);
    VerifyBodyPartWithOneOption(parts, eBodyPartS2C{hairPart}, m_hair, m_hairTexture, option, 0, 0, 0, false);
    SetHighByte(m_hairStyle, option);
  }
  // The inner layer.
  bool innerReset = false;  // the tail: sleeve style 3, the torso style recomputed
  if (m_innerTop < 0 || parts.GetNumBodyModels(eBodyPartS2C{7}) <= static_cast<unsigned int>(static_cast<int>(m_innerTop))) {
    AssertWithInfo("bad innerLayerTorso index");
    m_innerTopTexture = 0;
    m_innerTop = 0;
    innerReset = true;
  } else {
    if (static_cast<unsigned int>(m_innerOption1) > 1) {
      AssertWithInfo("Invalid innerLayerTorsoStyle");
      SetInnerLayerTorsoStyle(parts);
    }
    if (static_cast<unsigned int>(m_innerOption2) > 3) {
      AssertWithInfo("Invalid innerLayerSleeveStyle");
      m_innerOption2 = 3;
    }
    if (!parts.GetModelID(eBodyPartS2C{7}, static_cast<unsigned int>(GetBodyPartIndex(eBodyPartS2C{7})), GetBodyPartParam(eBodyPartS2C{7}))) {
      AssertWithInfo("innerLayerTorso is NULL");
      m_innerTopTexture = 0;
      m_innerTop = 0;
      m_innerOption2 = 3;
      SetInnerLayerTorsoStyle(parts);
    } else {
      const char* failed = nullptr;
      if (!parts.GetModelID(eBodyPartS2C{8}, static_cast<unsigned int>(GetBodyPartIndex(eBodyPartS2C{8})), GetBodyPartParam(eBodyPartS2C{8}))) {
        failed = "innerLayerSleeve is NULL";
      } else {
        const int count = static_cast<int>(parts.GetNumBodyTextures(eBodyPartS2C{7}, static_cast<unsigned int>(static_cast<int>(m_innerTop))));
        if (count == 0) {
          AssertWithInfo("innerLayerTorso texture list size is 0");
          m_innerTopTexture = 0;
          m_innerTop = 0;
          m_innerOption2 = 3;
          SetInnerLayerTorsoStyle(parts);
        } else if (m_innerTopTexture < 0 || count <= m_innerTopTexture) {
          AssertWithInfo("bad innerLayerTorso texture index");
          m_innerTopTexture = 0;
        }
        if (!parts.GetTextureID(eBodyPartS2C{7}, this, false)) {
          AssertWithInfo("innerLayerTorso texture is NULL");
          m_innerTopTexture = 0;
          m_innerTop = 0;
          m_innerOption2 = 3;
          SetInnerLayerTorsoStyle(parts);
        }
        if (!parts.GetTextureID(eBodyPartS2C{8}, this, false) && m_innerOption2 != 0) failed = "innerLayerSleeve texture is NULL";
      }
      if (failed) {
        AssertWithInfo(failed);
        m_innerTopTexture = 0;
        m_innerTop = 0;
        innerReset = true;
      }
    }
  }
  if (innerReset) {
    m_innerOption2 = 3;
    SetInnerLayerTorsoStyle(parts);
  }
  // The mid layer.
  if (m_midTop < 0 || parts.GetNumBodyModels(eBodyPartS2C{9}) <= static_cast<unsigned int>(static_cast<int>(m_midTop))) {
    AssertWithInfo("bad midLayerTorso index");
    m_midTopTexture = 0;
    m_midOption2 = 0;
    m_midOption3 = 3;
    m_midTop = 0;
    SetMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C{0}, parts);
    SetInnerLayerTorsoStyle(parts);
  } else {
    if (static_cast<unsigned int>(m_midOption1) > 5) {
      AssertWithInfo("Invalid midLayerTorsoStyle");
      SetMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C{0}, parts);
    }
    if (static_cast<unsigned int>(m_midOption2) > 1) {
      AssertWithInfo("Invalid midLayerCollarStyle");
      m_midOption2 = 0;
    }
    if (static_cast<unsigned int>(m_midOption3) > 3) {
      AssertWithInfo("Invalid midLayerSleeveStyle");
      m_midOption3 = 3;
    }
    auto resetMid = [&](const char* message) {
      AssertWithInfo(message);
      m_midOption2 = 0;
      m_midTopTexture = 0;
      m_midOption3 = 3;
      SetMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C{0}, parts);
      SetInnerLayerTorsoStyle(parts);
    };
    if (parts.GetModelID(eBodyPartS2C{9}, static_cast<unsigned int>(GetBodyPartIndex(eBodyPartS2C{9})), GetBodyPartParam(eBodyPartS2C{9}))) {
      const int count = static_cast<int>(parts.GetNumBodyTextures(eBodyPartS2C{9}, static_cast<unsigned int>(static_cast<int>(m_midTop))));
      if (count == 0) {
        resetMid("midLayerTorso texture list size is 0");
      } else if (m_midTopTexture < 0 || count <= m_midTopTexture) {
        AssertWithInfo("bad midLayerTorso texture index");
        m_midTopTexture = 0;
      }
      if (!parts.GetTextureID(eBodyPartS2C{9}, this, false)) resetMid("midLayerTorso texture is NULL");
    }
    if (parts.GetModelID(eBodyPartS2C{0xb}, static_cast<unsigned int>(GetBodyPartIndex(eBodyPartS2C{0xb})), GetBodyPartParam(eBodyPartS2C{0xb})) &&
        !parts.GetTextureID(eBodyPartS2C{0xb}, this, false))
      resetMid("midLayerSleeve texture is NULL");
  }
  // The outer layer.
  auto resetOuter = [&](const char* message) {
    AssertWithInfo(message);
    m_outerOption2 = 0;
    m_outerTop = 0;
    m_outerTopTexture = 0;
    m_outerOption1 = 0;
    m_outerOption3 = 3;
    SetMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C{m_midOption1}, parts);
    SetInnerLayerTorsoStyle(parts);
  };
  if (m_outerTop < 0 || parts.GetNumBodyModels(eBodyPartS2C{0xc}) <= static_cast<unsigned int>(static_cast<int>(m_outerTop))) {
    AssertWithInfo("bad outerLayerTorso index");
    m_outerTopTexture = 0;
    m_outerOption2 = 0;
    m_outerOption3 = 3;
    m_outerTop = 0;
    m_outerOption1 = 0;
    SetMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C{m_midOption1}, parts);
    SetInnerLayerTorsoStyle(parts);
  } else {
    if (static_cast<unsigned int>(m_outerOption1) > 1) {
      AssertWithInfo("bad outerLayerTorso style index");
      m_outerOption1 = 0;
    }
    if (static_cast<unsigned int>(m_outerOption2) > 3) {
      AssertWithInfo("bad outerLayerTorso collar style index");
      m_outerOption2 = 0;
    }
    if (static_cast<unsigned int>(m_outerOption3) > 3 || m_outerOption3 == 1) {
      AssertWithInfo("bad outerLayerTorso sleeve style index");
      m_outerOption3 = 3;
    }
    if (parts.GetModelID(eBodyPartS2C{0xc}, static_cast<unsigned int>(GetBodyPartIndex(eBodyPartS2C{0xc})), GetBodyPartParam(eBodyPartS2C{0xc}))) {
      const int count = static_cast<int>(parts.GetNumBodyTextures(eBodyPartS2C{0xc}, static_cast<unsigned int>(static_cast<int>(m_outerTop))));
      if (count == 0) {
        resetOuter("outerLayerTorso texture list size is 0");
      } else if (m_outerTopTexture < 0 || count <= m_outerTopTexture) {
        AssertWithInfo("bad outerLayerTorso texture index");
        m_outerTopTexture = 0;
      }
      if (!parts.GetTextureID(eBodyPartS2C{0xc}, this, false)) resetOuter("outerLayerTorso texture is NULL");
    }
    if (parts.GetModelID(eBodyPartS2C{0xe}, static_cast<unsigned int>(GetBodyPartIndex(eBodyPartS2C{0xe})), GetBodyPartParam(eBodyPartS2C{0xe})) &&
        !parts.GetTextureID(eBodyPartS2C{0xe}, this, false))
      resetOuter("outerLayerSleeve texture is NULL");
    if (parts.GetModelID(eBodyPartS2C{0xd}, static_cast<unsigned int>(GetBodyPartIndex(eBodyPartS2C{0xd})), GetBodyPartParam(eBodyPartS2C{0xd})) &&
        parts.OuterLayerCollarIsAHood(static_cast<unsigned int>(static_cast<int>(m_outerTop)), Sim__OuterLayerCollarStyleS2C{m_outerOption2}) &&
        !parts.GetTextureID(eBodyPartS2C{0xd}, this, false))
      resetOuter("outerLayerCollar texture is NULL");
  }
  {
    signed char option = HighByte(m_lowerBodyOption);
    VerifyBodyPartWithOneOption(parts, eBodyPartS2C{0xf}, m_lowerBody, m_lowerBodyTexture, option, 0, 0, 0, false);
    SetHighByte(m_lowerBodyOption, option);
  }
  VerifyBodyPartWithNoOptions(parts, eBodyPartS2C{0x10}, m_shoes, m_shoesTexture, 0, 0, false);
  if (m_bootTop != ShouldBootTopBeDisplayed(parts)) {
    AssertWithInfo("boot top displayed flag incorrectly set");
    SetShoeBoottopDisplayedFlag(parts);
  }
  VerifyBodyPartNoTexture(parts, eBodyPartS2C{0x12}, m_facialFeature, 0, true);
  VerifyBodyPartWithNoOptions(parts, eBodyPartS2C{0x16}, m_belt, m_beltTexture, 0, 0, true);
  VerifyBodyPartWithNoOptions(parts, eBodyPartS2C{0x19}, m_vestScarf, m_vestScarfTexture, 0, 0, true);
  {
    signed char option = HighByte(m_armAccessoryOption);
    VerifyBodyPartWithOneOption(parts, eBodyPartS2C{0x15}, m_armAccessory, m_armAccessoryTexture, option, 0, 0, 0, true);
    SetHighByte(m_armAccessoryOption, option);
  }
  VerifyBodyPartWithNoOptions(parts, eBodyPartS2C{0x14}, m_necklace, m_necklaceTexture, 0, 0, true);
  // Tattoos: below the number of tattoo records (all three places alike).
  for (int i = 0; i < 3; ++i) {
    const int t = static_cast<signed char>(m_tattoos[i]);
    if (t < 0 || static_cast<int>(parts.ArrayLength(4)) <= t) {
      AssertWithInfo("bad tattoo index");
      m_tattoos[i] = 0;
    }
  }
  if (m_morphBody < 0.0f || m_morphBody > 2.0f) AssertWithInfo("bad morphTorso value"), m_morphBody = 1.0f;
  if (m_morphArms < 0.0f || m_morphArms > 2.0f) AssertWithInfo("bad morphArms value"), m_morphArms = 1.0f;
  if (m_morphLegs < 0.0f || m_morphLegs > 2.0f) AssertWithInfo("bad morphLegs value"), m_morphLegs = 1.0f;
  if (m_morphHead < 0.0f || m_morphHead > 7.0f) AssertWithInfo("bad morphHead value"), m_morphHead = 3.0f;
  if (m_morph108 < 0.0f || m_morph108 > 3.0f) AssertWithInfo("bad morphEyes value"), m_morph108 = 1.0f;
  if (m_morph10C < 0.0f || m_morph10C > 3.0f) AssertWithInfo("bad morphNose value"), m_morph10C = 1.0f;
  if (m_morph110 < 0.0f || m_morph110 > 3.0f) AssertWithInfo("bad morphLips value"), m_morph110 = 1.0f;
  if (m_morph114 < 0.0f || m_morph114 > 3.0f) AssertWithInfo("bad morphChin value"), m_morph114 = 1.0f;
  bool personalityOk = true;
  for (int i = 0; i < 5; ++i)
    if (m_personality[i] >= 0xb) personalityOk = false;
  if (!personalityOk) {
    AssertWithInfo("personality values out of range");
    static const unsigned char kAries[5] = {8, 3, 3, 5, 6};  // zodiacPersonalityValues[0]
    for (int i = 0; i < 5; ++i) m_personality[i] = kAries[i];
  }
  if (m_zodiac > 0xb) {
    AssertWithInfo("bad aspiration value");
    m_zodiac = 0;
  }
  auto terminated = [](const unsigned short* name) {
    for (int i = 0; i < 0x20; ++i)
      if (!name[i]) return true;
    return false;
  };
  auto setDefault = [](unsigned short* name, const char* row) {
    const unsigned short* text = EGlobal::NewUIText(row);
    int i = 0;
    for (; i < 0x20 && text && text[i]; ++i) name[i] = text[i];  // wcsncpy 0x20
    for (; i < 0x20; ++i) name[i] = 0;
    name[0x1f] = 0;
  };
  if (!terminated(m_firstName)) {
    AssertWithInfo("first name is not null-terminated");
    setDefault(m_firstName, "default_player1_first_name");
  }
  if (!terminated(m_lastName)) {
    AssertWithInfo("last name is not null-terminated");
    setDefault(m_lastName, "default_player1_last_name");
  }
}

// 8016A670
unsigned int CasSimDescriptionS2C::GetCompositeTextureID(const char* name, eSimPartsMapLocation location) const {
  std::string n = GetCompositeTextureName(name, location);
  return EResourceManager::CalcId(n.data());
}

// 8016A6CC
// The name of a composited skin map of the sim called `name`: "af_cul_<name>" (gender, map).
std::string CasSimDescriptionS2C::GetCompositeTextureName(const char* name, eSimPartsMapLocation location) const {
  const char* gender = m_male ? "am" : "af";
  const char* map;
  switch (static_cast<int>(location)) {
    case 1: map = "cur"; break;
    case 2: map = "cll"; break;
    case 3: map = "clr"; break;
    default: map = "cul"; break;
  }
  char buf[256];
  std::snprintf(buf, sizeof buf, "%s_%s_%s", gender, map, name ? name : "");
  return buf;
}

// ---------------------------------------------------------------------------------------------------
// CasSimPartsS2C

// 8016A894
CasSimPartsS2C::CasSimPartsS2C() {
  m_quickdata = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(0x2a2af469u, nullptr, 0));
}

// 8016A8F8
CasSimPartsS2C::~CasSimPartsS2C() {
  m_row = nullptr;
  if (m_quickdata) {
    m_quickdata->DelRef();
    m_quickdata = nullptr;
  }
}

// 8016A950
void CasSimPartsS2C::SetGender(bool male) {
  m_male = male ? 1 : 0;
  if (!m_quickdata) return;
  const void* table = m_quickdata->getTable("Sim::SimPartsS2C");
  m_row = table ? m_quickdata->getRow(table, male ? "male" : "female") : nullptr;
}

const unsigned char* CasSimPartsS2C::Record(unsigned int off, unsigned int index, unsigned int stride) const {
  const auto* base = static_cast<const unsigned char*>(ERQuickdata::Ptr(m_row, off));
  return base ? base + index * stride : nullptr;
}

unsigned int CasSimPartsS2C::ArrayLength(unsigned int off) const {
  const auto* base = static_cast<const unsigned char*>(ERQuickdata::Ptr(m_row, off));
  return base ? ERQuickdata::U32(base - 4) : 0;
}

// 8016A9FC
// The length of the part's array in the gender's row.
unsigned int CasSimPartsS2C::GetNumBodyModels(eBodyPartS2C part) const {
  if (!m_row) return 0;
  const int p = part;
  unsigned int off;
  if (p == 0) off = 0x30;
  else if (p == 1) off = 0x1c;
  else if (p >= 2 && p <= 6) off = 0x20;
  else if (p == 7 || p == 8) off = 0x08;
  else if (p >= 9 && p <= 11) off = 0x0c;
  else if (p >= 12 && p <= 14) off = 0x10;
  else if (p == 15) off = 0x14;
  else if (p == 16 || p == 17) off = 0x18;
  else if (p == 18) off = 0x00;
  else if (p == 19) off = 0x24;
  else if (p == 20) off = 0x38;
  else if (p == 21) off = 0x3c;
  else if (p >= 22 && p <= 24) off = 0x28;
  else if (p == 25) off = 0x2c;
  else return 0;
  return ArrayLength(off);
}

// 8016AD5C
// The model id of style `index` of `part`, picked by the part's options (sleeve and collar styles,
// hair style, ...), as the game state wants it (CAS or "_LOD" model). 0: none.
unsigned int CasSimPartsS2C::GetModelID(eBodyPartS2C part, unsigned int index, Sim__SimPartsS2CParam param) const {
  if (!m_row || index >= GetNumBodyModels(part)) return 0;
  // A model id at `field` of the record of the part's array at `off`.
  auto id = [&](unsigned int off, unsigned int stride, unsigned int field) {
    const unsigned char* rec = Record(off, index, stride);
    return rec ? GetGameStateModelID(ERQuickdata::U32(rec, field)) : 0u;
  };
  const int o1 = param.m_option1, o2 = param.m_option2, o3 = param.m_option3;
  switch (static_cast<int>(part)) {
    case 0: return id(0x30, 0x18, 0);
    case 1: return id(0x1c, 0x0c, 0);
    case 2: return (o2 == 0 && o1 == 0) ? id(0x20, 0x28, 0) : 0;
    case 3: return o2 != 0 ? id(0x20, 0x28, 4) : 0;
    case 4: return (o2 == 0 && o1 == 1) ? id(0x20, 0x28, 8) : 0;
    case 5: return (o2 == 0 && o1 == 2) ? id(0x20, 0x28, 0xc) : 0;
    case 6: return (o2 == 0 && o1 == 3) ? id(0x20, 0x28, 0x10) : 0;
    case 7:
      if (o1 == 0) return id(0x08, 0x44, 0);
      if (o1 == 1) return id(0x08, 0x44, 0x38);
      return 0;
    case 8:
      if (o2 == 0) return m_male == 0 ? GetGameStateModelID(0x94656d22) : GetGameStateModelID(0xb744ea66);
      if (o2 == 1) return id(0x08, 0x44, 0x40);
      if (o2 == 2) return id(0x08, 0x44, 0x3c);
      if (o2 == 3) return id(0x08, 0x44, 4);
      return 0;
    case 9:
      switch (o1) {
        case 0: return id(0x0c, 0x68, 0);
        case 1: return id(0x0c, 0x68, 4);
        case 2: return id(0x0c, 0x68, 8);
        case 3: return id(0x0c, 0x68, 0xc);
        case 4: return id(0x0c, 0x68, 0x10);
        case 5: return id(0x0c, 0x68, 0x14);
        default: return 0;
      }
    case 10:
      if (o2 == 0) return id(0x0c, 0x68, 0x1c);
      if (o2 == 1) return id(0x0c, 0x68, 0x18);
      return 0;
    case 11:
      if (o3 == 1) return id(0x0c, 0x68, 0x28);
      if (o3 == 2) return id(0x0c, 0x68, 0x24);
      if (o3 == 3) return id(0x0c, 0x68, 0x20);
      return 0;
    case 12:
      if (o1 == 0) return id(0x10, 100, 4);
      if (o1 == 1) return id(0x10, 100, 0);
      return 0;
    case 13:
      if (o2 == 0) return id(0x10, 100, 8);
      if (o2 == 1) return id(0x10, 100, 0xc);
      if (o2 == 2) return id(0x10, 100, 0x4c);
      if (o2 == 3) return id(0x10, 100, 0x50);
      return 0;
    case 14:
      if (o3 == 2) return id(0x10, 100, 0x14);
      if (o3 == 3) return id(0x10, 100, 0x10);
      return 0;
    case 15:
      if (o1 == 0) return id(0x14, 0x3c, 0);
      if (o1 == 1) return id(0x14, 0x3c, 4);
      if (o1 == 2) return id(0x14, 0x3c, 8);
      if (o1 == 3) return id(0x14, 0x3c, 0xc);
      return 0;
    case 16: return id(0x18, 0x1c, 0);
    case 17: return o1 != 0 ? id(0x18, 0x1c, 4) : 0;
    case 18: return id(0x00, 0x18, 0);
    case 19:
      if (o1 == 0) return id(0x24, 0x24, 0);
      if (o1 == 1) return id(0x24, 0x24, 4);
      if (o1 == 2) return id(0x24, 0x24, 8);
      if (o1 == 3) return id(0x24, 0x24, 0xc);
      return 0;
    case 20: return id(0x38, 0x18, 0);
    case 21:
      if (o1 == 0) return id(0x3c, 0x20, 0);
      if (o1 == 1) return id(0x3c, 0x20, 4);
      if (o1 == 2) return id(0x3c, 0x20, 8);
      return 0;
    case 22: return id(0x28, 0x20, 0);
    case 23: return o1 != 0 ? id(0x28, 0x20, 4) : 0;
    case 24: return o2 != 0 ? id(0x28, 0x20, 8) : 0;
    case 25: return id(0x2c, 0x18, 0);
    default: return 0;
  }
}

// 8016CD58
int CasSimPartsS2C::OuterLayerTorsoHidesMusicPlayer(unsigned int index) const {
  const unsigned char* rec = Record(0x10, index, 100);
  return rec && rec[0x1d] ? 1 : 0;
}

// 8016CD80
// A mid (0x0B) or outer (0x0E) long sleeve style that leaves the arm accessory visible.
int CasSimPartsS2C::LongSleeveDoesNotHideArmAccessory(eBodyPartS2C part, unsigned int index) const {
  const unsigned char* rec;
  if (part == 0xb) rec = Record(0x0c, index, 0x68), rec = rec ? rec + 0x30 : nullptr;
  else if (part == 0xe) rec = Record(0x10, index, 100), rec = rec ? rec + 0x1e : nullptr;
  else return 0;
  return rec && *rec ? 1 : 0;
}

// 8016D508
unsigned int CasSimPartsS2C::GetGameStateModelID(unsigned int id) const {
  return SimModel::GetGameStateModelID(id, (m_lowRes ^ 1) != 0);
}

// 8016D534
unsigned int CasSimPartsS2C::GetGameStateTextureID(unsigned int id) const {
  return SimModel::GetGameStateTextureID(id, (m_lowRes ^ 1) != 0);
}

namespace {
// 803DFE65: the skin colours' names (eSkinColorType; 6..10 are the aged 0..4, 5 is alien).
const char* const kSkinColorNames[11] = {"caucasian", "asian", "brown", "darkbrown", "pale", "alien",
                                         "caucasian", "asian", "brown", "darkbrown", "pale"};

// The skin textures' suffix: "_buff" for the buff body (not aliens), "_old" for aged skins.
const char* SkinSuffix(int skin, int body) {
  if (static_cast<unsigned int>(skin - 6) < 5) return "_old";
  if (skin != 5 && body == 2) return "_buff";
  return "";
}

int SkinColorIndex(int skin) { return skin < 0 ? 0 : skin < 11 ? skin : 10; }
}  // namespace

// 8016B5E0
// The base skin of a map quadrant: "<af|am>_<ul|ur|ll|lr>_<colour><suffix>".
unsigned int CasSimPartsS2C::GetSkinTextureID(eSkinColorType skin, eBodyTypeS2C body, eSimPartsMapLocation location) {
  const int loc = location < 0 ? 0 : location < 4 ? static_cast<int>(location) : 3;
  char name[64];
  std::snprintf(name, sizeof name, "%s_%s_%s%s", m_male ? "am" : "af", SimModel::s_SimPartsMapNames[loc],
                kSkinColorNames[SkinColorIndex(skin)], SkinSuffix(skin, body));
  return GetGameStateTextureID(EResourceManager::CalcId(name));
}

// 8016B768
// The dataset holding the skin colour's base skins: "<af|am>_sk_<colour><suffix>".
unsigned int CasSimPartsS2C::GetSkinTexturesDatasetID(eSkinColorType skin, eBodyTypeS2C body) {
  char name[64];
  std::snprintf(name, sizeof name, "%s_sk_%s%s", m_male ? "am" : "af", kSkinColorNames[SkinColorIndex(skin)],
                SkinSuffix(skin, body));
  return GetGameStateTextureID(EResourceManager::CalcId(name));
}

// 8016B8BC (GetTextureIDFromVector)
unsigned int CasSimPartsS2C::TextureIDFromArray(const unsigned char* rec, unsigned int field, int texture) {
  if (!rec) return 0;
  const auto* ids = static_cast<const unsigned char*>(ERQuickdata::Ptr(rec, field));
  if (!ids || texture < 0) return 0;
  const unsigned int count = ERQuickdata::U32(ids - 4);
  return static_cast<unsigned int>(texture) < count ? ERQuickdata::U32(ids, static_cast<unsigned int>(texture) * 4) : 0u;
}

// 8016B910
// The texture (or its reflection mask) of the part's style, picked by the part's options like its
// model; texture index from the description. 0: none (no model, or a style without its own texture).
unsigned int CasSimPartsS2C::GetTextureID(eBodyPartS2C part, CasSimDescriptionS2C* desc, bool reflection) const {
  if (!m_row || !desc) return 0;
  const unsigned int index = static_cast<unsigned int>(desc->GetBodyPartIndex(part)) & 0xff;
  if (index >= GetNumBodyModels(part)) return 0;
  const Sim__SimPartsS2CParam param = desc->GetBodyPartParam(part);
  const int p = part;
  if (!GetModelID(part, index, param) && p != eBodyPartS2C::kFacialFeature) return 0;
  const int o1 = param.m_option1, o2 = param.m_option2, o3 = param.m_option3;
  int texture = param.m_texture;
  unsigned int off = 0, stride = 0, tex = 0, refl = 0;  // the parts array, its record size, the id arrays
  auto set = [&](unsigned int o, unsigned int s, unsigned int t, unsigned int r) { off = o, stride = s, tex = t, refl = r; };
  switch (p) {
    case 0: set(0x30, 0x18, 0x10, 0x14); texture = desc->m_glassesTexture; break;
    case 2: if (o2 || o1) return 0; set(0x20, 0x28, 0x18, 0x1c); break;
    case 3: if (!o2) return 0; set(0x20, 0x28, 0x18, 0x1c); break;
    case 4: if (o2 || o1 != 1) return 0; set(0x20, 0x28, 0x18, 0x1c); break;
    case 5: if (o2 || o1 != 2) return 0; set(0x20, 0x28, 0x18, 0x1c); break;
    case 6: if (o2 || o1 != 3) return 0; set(0x20, 0x28, 0x18, 0x1c); break;
    case 7: set(0x08, 0x44, 0x0c, 0x10); break;
    case 8:
      if (o2 == 2) set(0x08, 0x44, 0x1c, 0x20);
      else if (o2 == 1) set(0x08, 0x44, 0x24, 0x28);
      else if (o2 == 3) set(0x08, 0x44, 0x14, 0x20);
      else return 0;
      break;
    case 9:
      if (o1 == 2 || o1 == 5) set(0x0c, 0x68, 0x34, 0x38);
      else if (o1 >= 0 && o1 <= 4) set(0x0c, 0x68, 0x3c, 0x40);
      else return 0;
      break;
    case 11:
      if (o3 == 1) set(0x0c, 0x68, 0x54, 0x58);
      else if (o3 == 2) set(0x0c, 0x68, 0x4c, 0x50);
      else if (o3 == 3) set(0x0c, 0x68, 0x44, 0x48);
      else return 0;
      break;
    case 12:
      if (OuterLayerTorsoIsAHood(index, Sim__OuterLayerTorsoStyleS2C{o1})) set(0x10, 100, 0x58, 0x5c);
      else if (o1 == 0) set(0x10, 100, 0x20, 0x24);
      else if (o1 == 1) set(0x10, 100, 0x28, 0x2c);
      else return 0;
      break;
    case 13:
      if (!OuterLayerCollarIsAHood(index, Sim__OuterLayerCollarStyleS2C{o2})) return 0;
      set(0x10, 100, 0x58, 0x5c);
      break;
    case 14:
      if (o3 == 2) set(0x10, 100, 0x38, 0x3c);
      else if (o3 == 3) set(0x10, 100, 0x30, 0x34);
      else return 0;
      break;
    case 15:
      if (o1 < 0 || o1 > 3) return 0;
      set(0x14, 0x3c, 0x14 + o1 * 8, 0x18 + o1 * 8);
      break;
    case 17: if (!o1) return 0; set(0x18, 0x1c, 0x0c, 0x10); break;
    case 16: set(0x18, 0x1c, 0x0c, 0x10); break;
    case 18: set(0x00, 0x18, 0x10, 0x14); break;
    case 19: set(0x24, 0x24, 0x14, 0x18); break;
    case 20: set(0x38, 0x18, 0x10, 0x14); break;
    case 21: set(0x3c, 0x20, 0x18, 0x1c); break;
    case 23: if (!o1) return 0; set(0x28, 0x20, 0x10, 0x14); break;
    case 24: if (!o2) return 0; set(0x28, 0x20, 0x10, 0x14); break;
    case 22: set(0x28, 0x20, 0x10, 0x14); break;
    case 25: set(0x2c, 0x18, 0x10, 0x14); break;
    default: return 0;  // the head (its skin is the base skin) and the mid collar
  }
  return GetGameStateTextureID(TextureIDFromArray(Record(off, index, stride), reflection ? refl : tex, texture));
}

// 8016C82C
unsigned int CasSimPartsS2C::GetReflectionTextureID(eBodyPartS2C part, CasSimDescriptionS2C* desc) const {
  return GetTextureID(part, desc, true);
}

// 8016CDD8
int CasSimPartsS2C::OuterLayerCollarIsAHood(unsigned int index, Sim__OuterLayerCollarStyleS2C style) const {
  const int s = style;
  if (s < 0 || s > 3) return 0;
  const unsigned char* rec = Record(0x10, index, 100);
  return rec && rec[0x54 + s] ? 1 : 0;
}

// 8016CE80
int CasSimPartsS2C::OuterLayerTorsoIsAHood(unsigned int index, Sim__OuterLayerTorsoStyleS2C style) const {
  const int s = style;
  if (s < 0 || s > 1) return 0;
  const unsigned char* rec = Record(0x10, index, 100);
  return rec && rec[0x60 + s] ? 1 : 0;
}

// ---------------------------------------------------------------------------------------------------
// SimModel

// 80070580
SimModel::SimModel() {
  gSimPartsMapSizeMultiplier_CAS = s_forceLowResSims ? 1 : 2;
}

// 8007068C
SimModel::~SimModel() { Reset(); }

// 80070718
void SimModel::Init(CasSimDescriptionS2C* desc, bool highRes) {
  Reset();
  desc->Verify();
  m_simParts = new CasSimPartsS2C();
  SetHighResolution(highRes);
  SetSimDescription(desc);
}

// 8007077C
void SimModel::Reset() {
  FlushLoadingModels();
  DeallocateAllModels();
  DeallocateAllSkinTextures();
  delete m_simParts;
  m_simParts = nullptr;
  m_highRes = 1;
  if (m_ownsDesc && m_desc) {
    delete m_desc;
    m_desc = nullptr;
  }
  m_ownsDesc = 0;
  m_desc = nullptr;
}

// 80070808
void SimModel::DeallocateAllModels() {
  for (int p = 0; p < eBodyPartS2C::kCount; ++p) DeallocateModelPart(eBodyPartS2C{p});
}

// 8007084C
// Also drops the model's morph targets unless the model is shared (flag 4).
void SimModel::DeallocateModelPart(eBodyPartS2C part) {
  const int p = part;
  if (EIStaticModel* m = m_parts[p]) {
    if (m->m_model && !(m_partFlags[p] & kShared)) m->m_model->CleanMorphTargets(false);
    m->SafeDelete();
    m_parts[p] = nullptr;
  }
  m_partFlags[p] &= ~0x1eu;
}

// 800708F4
// The morph data of every morphing part is freed; the parts keep their shapes and count as shared
// (flag 4) from then on, so nothing morphs them again.
void SimModel::DeallocateMorphResources() {
  for (int p = 0; p < eBodyPartS2C::kCount; ++p) {
    ERModel* m = m_parts[p] ? m_parts[p]->m_model : nullptr;
    if (m && m->GetNumMorphBases()) {
      m_partFlags[p] |= kShared;
      m->FreeAllMorphAndLatticeResource(true);
    }
  }
}

// 8007096C
void SimModel::DeallocateAllSkinTextures() {
  for (ERTexture*& t : m_skin) {
    if (t) {
      _textureman.DelRef(t, EResourceManager__DelRefMode{0});
      t = nullptr;
    }
  }
}

// 800709D0
void SimModel::SetSimDescription(CasSimDescriptionS2C* desc) {
  if (!desc) return;
  if (m_ownsDesc && m_desc) delete m_desc;
  m_desc = desc;
  m_simParts->SetGender(desc->m_male != 0);
}

// 80070A34
void SimModel::SetHighResolution(bool highRes) {
  m_highRes = highRes ? 1 : 0;
  m_simParts->m_lowRes = highRes ? 0 : 1;
}

// 80070CC4
// Every part from the description, outermost layers first.
void SimModel::SetAllModels() {
  UpdateHead(true, false, false);
  UpdateOuterLayerCollar(false, false);
  UpdateOuterLayerTorso(true, false, false, false);
  UpdateOuterLayerSleeve(true, false, false, false);
  UpdateMidLayerCollar(false);
  UpdateMidLayerTorso(true, false, false, false);
  UpdateMidLayerSleeve(true, false, false, false);
  UpdateInnerLayerTorso(true, false, false);
  UpdateInnerLayerSleeve(true, false, false);
  UpdateLowerBody(true, false);
  UpdateShoes(true, false, false);
  UpdateVestScarf(true, false, false);
  UpdateHat(true, false, false);
  UpdateHair(true, false, false);
  UpdateArmAccessory(true, false, false);
  UpdateBelt(true, false, false);
  UpdateGlasses(true, false, false);
  UpdateFacialFeature(true, false, false);
  UpdateNecklaceEarring(true, false, false);
}

// 80070E64
// A new EIStaticModel of `id` for the part (none for 0); the head takes its morph targets.
int SimModel::SetModelPart(eBodyPartS2C part, unsigned int id) {
  const int p = part;
  int changed = 0;
  if (EIStaticModel* m = m_parts[p]) {
    if (m->m_model && id == m->m_model->m_id) return 0;
    changed = 1;
  }
  DeallocateModelPart(part);
  const unsigned int flags = m_partFlags[p] & ~0xcu;
  m_partFlags[p] = flags | kChanged;
  if (!id) return changed;
  m_partFlags[p] = flags | kChanged | kHasModel;
  auto* model = new EIStaticModel();  // GC: 800 bytes, 16-aligned, from the main heap
  m_parts[p] = model;
  model->SetModel(id, false, nullptr, nullptr);
  model->Init();
  if (model->m_model && model->m_model->m_manager) model->m_model->Detach();
  if (p == eBodyPartS2C::kHead && m_highRes) LoadAllMorphTargets();
  return 1;
}

// 80070FB4
int SimModel::UpdateModelPart(eBodyPartS2C part, bool lattice, bool composite) {
  const Sim__SimPartsS2CParam param = m_desc->GetBodyPartParam(part);
  const unsigned int index = static_cast<unsigned int>(m_desc->GetBodyPartIndex(part));
  return ChangeModelPart(part, m_simParts->GetModelID(part, index, param), lattice, composite);
}

// 8007115C
// The part's model becomes `id` (or goes for 0): welded to the parts it joins, lattice-morphed, skinned.
int SimModel::ChangeModelPart(eBodyPartS2C part, unsigned int id, bool lattice, bool composite) {
  const int p = part;
  if (!id) {
    if (!m_parts[p]) return 0;
    DeallocateModelPart(part);
    return 1;
  }
  if (m_parts[p] && m_parts[p]->m_model && m_parts[p]->m_model->m_id == id) return 0;
  const int changed = SetModelPart(part, id);
  unsigned int joins[7];
  m_simParts->GetWeldableBodyPartConnections(part, joins, 7);
  for (unsigned int other : joins)
    if (other != eBodyPartS2C::kCount) WeldSharedVertices(part, eBodyPartS2C{static_cast<int>(other)}, false);
  if (lattice) ApplyLatticeMorphingToModelPart(part);
  ApplySkinToModelPart(part);
  RebuildModified();
  if (composite) SIMMY_STUB("SimModel::ChangeModelPart: CompositeSkinPart", 0x8007115C);
  return changed;
}

// 80071558
// Both parts loaded and not shared: their shared border vertices welded (tolerance 0.001), both marked
// changed (with `rebuild`, GC's ERModel::RebuildAfterWelding instead, which is empty).
void SimModel::WeldSharedVertices(eBodyPartS2C part, eBodyPartS2C other, bool rebuild) {
  EIStaticModel* a = m_parts[part];
  EIStaticModel* b = m_parts[other];
  if (!a || !a->m_model || (m_partFlags[part] & kShared)) return;
  if (!b || !b->m_model || (m_partFlags[other] & kShared)) return;
  a->m_model->WeldSharedVerts(b->m_model, 0.001f);
  if (!rebuild) {
    m_partFlags[part] |= kChanged;
    m_partFlags[other] |= kChanged;
  } else {
    ERModel::RebuildAfterWelding();
    ERModel::RebuildAfterWelding();
  }
}

// 80071434
int SimModel::IsLoadingModels() const {
  for (unsigned int f : m_partFlags)
    if (f & kLoading) return 1;
  return 0;
}

// 80071460
void SimModel::FlushLoadingModels() {
  for (int p = 0; p < eBodyPartS2C::kCount; ++p) {
    if (m_pendingIds[p]) _modelman.DelRefAsync(m_pendingIds[p]);
    m_pendingIds[p] = 0;
    m_partFlags[p] &= ~kLoading;
  }
}

// 80071620
// Every part's lattices from the description's body sliders (the parts not shared with another sim;
// GC table 803D557C: every part but the shoes).
void SimModel::ApplyAllLatticeMorphing() {
  static const int kParts[25] = {0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 20, 21, 22, 23, 24, 25, 2, 3, 4, 5, 6, 18, 19, 1};
  EIStaticModel* parts[eBodyPartS2C::kCount] = {};
  for (int p : kParts) {
    if (!(m_partFlags[p] & kShared)) parts[p] = m_parts[p];
    m_partFlags[p] |= kChanged;
  }
  MorphUtilities morph(parts);
  morph.ApplyAllMorphs(*m_desc);
}

// 8007172C
// One part's lattices after it changed, with the sliders that shape it; the parts that sit on it
// (worn over it or joined to it) are morphed again with it.
void SimModel::ApplyLatticeMorphingToModelPart(eBodyPartS2C part) {
  const int p = part;
  if (!m_parts[p]) return;
  EIStaticModel* parts[eBodyPartS2C::kCount] = {};
  bool head = false, body = false, arms = false, legs = false;
  // The groups of parts the GC copied for each kind of part.
  static const int kHeadParts[] = {1, 0, 2, 3, 4, 5, 6, 18, 19, 20, 24};
  static const int kBodyParts[] = {7, 9, 12, 10, 13, 22, 23, 25, 24};
  static const int kArmParts[] = {8, 11, 14, 21};
  auto take = [&](std::initializer_list<int> list) {
    for (int q : list) parts[q] = m_parts[q];
  };
  auto takeArray = [&](const int* list, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) parts[list[i]] = m_parts[list[i]];
  };
  if (p == 10 || p == 13 || (p >= 22 && p <= 25)) {  // collars, belts, vest: the body
    body = true;
    takeArray(kBodyParts, 9);
  } else if ((p >= 0 && p <= 6) || (p >= 18 && p <= 20)) {  // head and what is worn on it
    head = true;
    takeArray(kHeadParts, 11);
  } else if (p == 7 || p == 9 || p == 12) {  // torsos: head and body
    head = body = true;
    takeArray(kHeadParts, 11);
    takeArray(kBodyParts, 9);
    take({15});
  } else if (p == 15) {  // lower body: body and legs
    body = legs = true;
    take({15, 17, 1, 7, 9, 12, 20, 24, 10, 13, 22, 23});
  } else if (p == 17) {  // boot tops: legs
    legs = true;
    take({15, 17});
  } else if (p == 8 || p == 11 || p == 14 || p == 21) {  // sleeves, arm accessory: arms
    arms = true;
    takeArray(kArmParts, 4);
  }
  for (int q = 0; q < eBodyPartS2C::kCount; ++q) {
    if (!parts[q]) continue;
    if (m_partFlags[q] & kShared) parts[q] = nullptr;
    else m_partFlags[q] |= kChanged;
  }
  MorphUtilities morph(parts);
  if (head) morph.ApplyMorphHead(m_desc->m_morphHead);
  if (body) morph.ApplyMorph(0, m_desc->m_morphBody);
  if (arms) morph.ApplyMorph(1, m_desc->m_morphArms);
  if (legs) morph.ApplyMorph(3, m_desc->m_morphLegs);
}

namespace {
// The head's morph target model ids (the head style's record in the gender's parts row, +8), for the
// sim's resolution, packed into the 12 slots: a head with 14 has two unused ones (9 and 10).
int HeadMorphTargetIds(const SimModel& m, unsigned int ids[12]) {
  for (int i = 0; i < 12; ++i) ids[i] = 0;
  const unsigned char* rec = m.m_simParts->Record(0x1c, static_cast<unsigned int>(m.m_desc->m_head), 0xc);
  const auto* list = rec ? static_cast<const unsigned char*>(ERQuickdata::Ptr(rec, 8)) : nullptr;
  const int count = list ? static_cast<int>(ERQuickdata::U32(list - 4)) : 0;
  if (!count) return 0;
  const bool highRes = const_cast<SimModel&>(m).UsingHighResolutionModel() != 0;
  unsigned int n = 0;
  for (int i = 0; i < count; ++i) {
    if (count == 14 && (i == 9 || i == 10)) continue;
    if (n < 12) ids[n] = SimModel::GetGameStateModelID(ERQuickdata::U32(list, static_cast<unsigned int>(i) * 4), highRes);
    ++n;  // GC: more than 12 overran its array
  }
  return count;
}
}  // namespace

// 80071B1C
// The face sliders on the head. With the morph targets of the head's style each face region (0x108
// chin.., 0x10C, 0x110, 0x114) whose slider is off its middle registers its two targets, adds their
// offsets and lets them go again; the head is then finished by SimRenderer::MorphParts.
void SimModel::ApplyAllMorphTargets() {
  ERModel* model = m_parts[eBodyPartS2C::kHead] ? m_parts[eBodyPartS2C::kHead]->m_model : nullptr;
  if (!model || (m_partFlags[eBodyPartS2C::kHead] & kShared)) return;
  m_partFlags[eBodyPartS2C::kHead] |= kChanged;
  model->ResetMorph();
  unsigned int ids[12];
  if (HeadMorphTargetIds(*this, ids)) {
    if (m_desc->m_morph108 != 1.0f) ApplyMorphTargetsToRegion(0, 3, m_desc->m_morph108, &ids[0]);
    if (m_desc->m_morph114 != 1.0f) ApplyMorphTargetsToRegion(9, 3, m_desc->m_morph114, &ids[9]);
    if (m_desc->m_morph10C != 1.0f) ApplyMorphTargetsToRegion(6, 3, m_desc->m_morph10C, &ids[6]);
    if (m_desc->m_morph110 != 1.0f) ApplyMorphTargetsToRegion(3, 3, m_desc->m_morph110, &ids[3]);
    model->MorphLattices();
    model->MorphTargets();
    return;
  }
  model->MorphLattices();
  model->MorphTargets();
  model->ApplyMorph();
}

// 80071E18
// One face region: its slider's one or two targets registered in slots base.., weighted, added.
void SimModel::ApplyMorphTargetsToRegion(unsigned int base, unsigned int count, float value, unsigned int* ids) {
  ERModel* model = m_parts[eBodyPartS2C::kHead] ? m_parts[eBodyPartS2C::kHead]->m_model : nullptr;
  if (!model || (m_partFlags[eBodyPartS2C::kHead] & kShared)) return;
  m_partFlags[eBodyPartS2C::kHead] |= kChanged;
  MorphUtilities morph(m_parts);
  float w1 = 0.0f, w2 = 0.0f;
  int i1 = -1, i2 = -1;
  morph.GetFacialMorphTargetIndexAndValue(value, w1, i1, w2, i2);
  if (w1 == 0.0f) i1 = -1;
  if (w2 == 0.0f) i2 = -1;
  std::vector<ERModel*> registered(count, nullptr);  // GC: from the VM heap
  for (unsigned int u = 0; u < count; ++u) {
    if (ids[u] && (static_cast<int>(u) == i1 || static_cast<int>(u) == i2)) {
      registered[u] = model->RegisterMorphTarget(ids[u], static_cast<int>(base + u));
      if (registered[u]) registered[u]->GenerateMorphTargetDeltas(model);
    }
  }
  if (i1 != -1) model->SetMorphTargetWeight(static_cast<unsigned int>(i1) + base, w1);
  if (i2 != -1) model->SetMorphTargetWeight(static_cast<unsigned int>(i2) + base, w2);
  model->MorphTargets();
  for (ERModel* t : registered)
    if (t) model->UnRegisterMorphTarget(t);
}

// 800723C8
void SimModel::ApplySkinToModels() {
  for (int p = 0; p < eBodyPartS2C::kCount; ++p) ApplySkinToModelPart(eBodyPartS2C{p});
}

// 8007240C
// The part's shaders take the composited skin map of its quadrant (a hood's is map 2) in place of
// their base texture; the hands (shader 0x568ca34b/0x75ad240f, on the hair parts) and the head's
// second shader (0x4cc81175/0xb428ea82, on the facial feature and hat) take the head's or the hair's
// map. Then the part's uvs are moved into its rectangle of the map.
void SimModel::ApplySkinToModelPart(eBodyPartS2C part) {
  const int p = part;
  if (!m_parts[p] || (m_partFlags[p] & (kShared | kSkinApplied))) return;
  unsigned int location = s_SimPartsMap[p].m_location;
  unsigned int secondShader = 0;
  int secondPart = 1;
  if (p == eBodyPartS2C::kOuterTorso) {
    if (OuterTorsoIsAHood()) location = 2;
  } else if (p == eBodyPartS2C::kOuterCollar) {
    if (OuterCollarIsAHood()) location = 2;
  } else if (p >= 2 && p <= 6) {
    secondShader = m_desc->m_male ? 0x75ad240fu : 0x568ca34bu;
  } else if (p == 0x12 || p == 0x13) {
    secondShader = m_desc->m_male ? 0xb428ea82u : 0x4cc81175u;
    secondPart = 2;
  }
  ERTexture* second = secondShader ? m_skin[s_SimPartsMap[secondPart].m_location] : nullptr;
  if (m_skin[location]) m_parts[p]->ReplaceBaseTexture(m_skin[location], secondShader, second);
  TransformModelPartUV(part);
  m_partFlags[p] |= kSkinApplied;
}

// 800725D4
void SimModel::TransformAllModelUV() {
  for (int p = 0; p < eBodyPartS2C::kCount; ++p) TransformModelPartUV(eBodyPartS2C{p});
}

// 80072618
void SimModel::TransformModelPartUV(eBodyPartS2C part) {
  EIStaticModel* m = m_parts[part];
  if (!m || !m->m_model) return;
  EVec2 scale, offset, secondScale, secondOffset;
  unsigned int secondShader = 0;
  GetModelUVTransformParameters(part, scale, offset, secondShader, secondScale, secondOffset);
  m->m_model->TransformModelUV(scale, offset, secondShader, secondScale, secondOffset);
}

// 800726A8
// The part's rectangle of its skin map as a uv scale and offset (map units of 128); for the parts
// with a second skinned shader also that one's (the hands' or the head's rectangle).
void SimModel::GetModelUVTransformParameters(eBodyPartS2C part, EVec2& scale, EVec2& offset, unsigned int& secondShader,
                                             EVec2& secondScale, EVec2& secondOffset) {
  const int p = part;
  scale = offset = secondScale = secondOffset = EVec2{0.0f, 0.0f};
  secondShader = 0;
  const SimPartsMapEntry& e = s_SimPartsMap[p];
  unsigned int x0 = e.m_x0, y0 = e.m_y0, x1 = e.m_x1, y1 = e.m_y1;
  unsigned int sx0 = 0, sx1 = 0x80, sy1 = 0x80;
  if ((p == eBodyPartS2C::kOuterTorso && OuterTorsoIsAHood()) || (p == eBodyPartS2C::kOuterCollar && OuterCollarIsAHood())) {
    x0 = y0 = 0x40;
    x1 = y1 = 0x80;
  } else if (p >= 2 && p <= 6) {
    secondShader = m_desc->m_male ? 0x75ad240fu : 0x568ca34bu;
    sx0 = 0, sx1 = 0x40, sy1 = 0x40;
  } else if (p == 0x12 || p == 0x13) {
    secondShader = m_desc->m_male ? 0xb428ea82u : 0x4cc81175u;
    sx0 = 0x40, sx1 = 0x80, sy1 = 0x40;
  }
  const float k = 1.0f / 128.0f;
  scale = EVec2{static_cast<float>((x1 - x0) & 0xffff) * k, static_cast<float>((y1 - y0) & 0xffff) * k};
  offset = EVec2{static_cast<float>(x0) * k, static_cast<float>(y0) * k};
  if (secondShader) {
    secondScale = EVec2{static_cast<float>((sx1 - sx0) & 0xffff) * k, static_cast<float>(sy1) * k};
    secondOffset = EVec2{static_cast<float>(sx0) * k, 0.0f};
  }
}

// 80072B20
// Every quadrant of the skin, with the skin colour's base skins loaded meanwhile.
void SimModel::CompositeAllSkin() {
  if (!m_desc) return;
  EResource* dataset = nullptr;
  if (!m_desc->m_precomposited)
    dataset = _datasetman.AddRef(m_simParts->GetSkinTexturesDatasetID(eSkinColorType{m_desc->m_skinColor},
                                                                      eBodyTypeS2C{m_desc->m_bodyType}),
                                 nullptr, 0);
  for (unsigned char q = 0; q < 4; ++q) CompositeSkinPart(q);
  if (dataset) _datasetman.DelRef(dataset, EResourceManager__DelRefMode{1});
}

// 80072BE4
// One quadrant: a saved sim's precomposited map when there is one, otherwise composited from the parts.
void SimModel::CompositeSkinPart(unsigned char quadrant) {
  SkinCompositor compositor;
  if (!m_desc || quadrant >= 4) return;
  if (m_desc->m_precomposited) {
    const unsigned int id = GetGameStateTextureID(EResourceManager::CalcId(m_skinNames[quadrant].data()), m_highRes != 0);
    if (compositor.CreateSkinQuadrantPrecomposited(m_skin[quadrant], id, m_highRes != 0)) return;
  }
  if (!m_highRes) compositor.CreateSkinQuadrantLoRes(m_skin[quadrant], quadrant, m_desc);
  else compositor.CreateSkinQuadrantHiRes(m_skin[quadrant], quadrant, m_desc);
}

// 80072064
// The four skin textures the SkinCompositor draws into, named after the sim ("af_cul_<name>"; a
// number is added while the name is taken).
int SimModel::CreateSkin(char* name) {
  DeallocateAllSkinTextures();
  ETextureDef def;
  GetSkinTextureDef(def, m_highRes != 0);
  for (unsigned int q = 0; q < 4; ++q) {
    const eSimPartsMapLocation loc{static_cast<int>(q)};
    unsigned int id = m_desc->GetCompositeTextureID(name, loc);
    m_skinNames[q] = m_desc->GetCompositeTextureName(name, loc);
    if (_textureman.IsLoaded(id)) {
      unsigned int n = 2, retries = 0;
      char unique[256];
      do {
        if (n > 99) {
          if (retries) {
            ++retries;
            break;
          }
          retries = 1;
          _textureman.FreeUnreferencedResources();
          n = 2;
        }
        std::snprintf(unique, sizeof unique, "%s%u", name, n++);
        id = m_desc->GetCompositeTextureID(unique, loc);
      } while (_textureman.IsLoaded(id));
      if (retries > 1) return 0;
      m_skinNames[q] = m_desc->GetCompositeTextureName(unique, loc);
    }
    def.m_name = "*simskin*";  // GC: the EString of m_skinNames
    ETexture* texture = _pGfx->CreateTexture(def);
    if (!texture) return 0;
    auto* res = new ERTexture();  // GC: 0x18 bytes from the texture manager's heap
    res->Attach(texture);
    res->m_flags = ((def.m_width * def.m_height * (def.m_bpp >> 3) + def.m_paletteSize * (def.m_paletteBpp >> 3)) << 8) |
                   (res->m_flags & 0xff);
    _textureman.AddResource(res, id);
    m_skin[q] = res;
  }
  return 1;
}

// 80072354
// The skin maps: 128 pixels square per multiplier (the CAS's 2 for high resolution sims).
// PC: 32-bit RGBA (format 0x85) instead of the GameCube's 8-bit palettized (0x8A, quantised by
// TextureUtils::CopyTexture32To8 from the compositor's 32-bit scratch map).
void SimModel::GetSkinTextureDef(ETextureDef& def, bool highRes) {
  const short multiplier = highRes ? gSimPartsMapSizeMultiplier_CAS : gSimPartsMapSizeMultiplier_Game;
  def.m_width = static_cast<unsigned short>(multiplier << 7);
  def.m_height = static_cast<unsigned short>(multiplier << 7);
  def.m_format = 0x85;
  def.m_bpp = 0x20;
  def.m_paletteFormat = 0;
  def.m_paletteSize = 0;
  def.m_paletteBpp = 0;
  def.m_flags = (def.m_flags & ~3u) | 0x80;
}

// 80072CBC
// After the welds: the head (unless shared) gets its morphs applied, the face sliders' targets and the
// head-shape lattice that ApplyAllMorphTargets left pending; with `final` the other parts are morphed
// too (the head stays as it is now) and the morph data is let go.
void SimModel::UpdateReweld(bool final) {
  RebuildModified();
  ERModel* model = m_parts[eBodyPartS2C::kHead] ? m_parts[eBodyPartS2C::kHead]->m_model : nullptr;
  if (!model) return;
  if (!(m_partFlags[eBodyPartS2C::kHead] & kShared)) {
    model->ApplyMorph();
    m_partFlags[eBodyPartS2C::kHead] |= kChanged;
  }
  if (final) {
    SimRenderer::MorphParts(m_parts, false);
    DeallocateMorphResources();
  }
}

// 80072D3C
// CAS: the same with the head morphed by MorphParts (CAS keeps morphing it as the sliders move).
void SimModel::UpdateReweldCAS(bool final) {
  RebuildModified();
  if (!(m_partFlags[eBodyPartS2C::kHead] & kShared) && final) {
    SimRenderer::MorphParts(m_parts, true);
    DeallocateMorphResources();
  }
}

// 80072E30
// Changed (welded) parts' models are rebuilt (ERModel::RebuildAfterWelding, empty on GC: the welded
// vertex arrays are drawn by reference, on PC too), and the marks cleared.
void SimModel::RebuildModified() {
  for (int p = 0; p < eBodyPartS2C::kCount; ++p) {
    if (m_parts[p] && m_parts[p]->m_model && (m_partFlags[p] & kChanged) && !(m_partFlags[p] & kShared))
      ERModel::RebuildAfterWelding();
    m_partFlags[p] &= ~kChanged;
  }
}

// 80072EB8
// CAS heads keep all their face morph targets registered (the sliders only change weights).
void SimModel::LoadAllMorphTargets() {
  unsigned int ids[12];
  if (HeadMorphTargetIds(*this, ids) <= 11) return;
  ERModel* model = m_parts[eBodyPartS2C::kHead]->m_model;
  for (int u = 0; u < 12; ++u) {
    if (!ids[u]) continue;
    if (ERModel* t = model->RegisterMorphTarget(ids[u], u)) t->GenerateMorphTargetDeltas(model);
  }
}

// 8007302C
// CAS models are used as they are; the game's are their "_LOD" versions.
unsigned int SimModel::GetGameStateModelID(unsigned int id, bool highRes) {
  if (!id) return 0;
  if (!s_forceLowResSims && highRes) return id;
  return static_cast<unsigned int>(EChecksum::ComputeSymbol(const_cast<char*>("_LOD"), id));
}

// 80073090
// The texture as the game state wants it: the CAS's own, or the game's ("_GAME") version.
unsigned int SimModel::GetGameStateTextureID(unsigned int id, bool highRes) {
  if (!id) return 0;
  if (!s_forceLowResSims && highRes) return id;
  return static_cast<unsigned int>(EChecksum::ComputeSymbol(const_cast<char*>("_GAME"), id));
}

int SimModel::OuterTorsoIsAHood() const {
  return m_simParts->OuterLayerTorsoIsAHood(static_cast<unsigned int>(m_desc->GetBodyPartIndex(eBodyPartS2C{12})),
                                            Sim__OuterLayerTorsoStyleS2C{m_desc->GetBodyPartParam(eBodyPartS2C{12}).m_option1});
}

int SimModel::OuterCollarIsAHood() const {
  return m_simParts->OuterLayerCollarIsAHood(static_cast<unsigned int>(m_desc->GetBodyPartIndex(eBodyPartS2C{13})),
                                             Sim__OuterLayerCollarStyleS2C{m_desc->GetBodyPartParam(eBodyPartS2C{13}).m_option2});
}

// 80485698 (made by the static initialiser): each part's map quadrant and rectangle.
const SimPartsMapEntry SimModel::s_SimPartsMap[eBodyPartS2C::kCount] = {
    {0, 2, 0, 0x20, 0x20, 0x40},      {1, 0, 0, 0, 0x40, 0x40},         {2, 2, 0x40, 0, 0x80, 0x40},
    {3, 2, 0x40, 0, 0x80, 0x40},      {4, 2, 0x40, 0, 0x80, 0x40},      {5, 2, 0x40, 0, 0x80, 0x40},
    {6, 2, 0x40, 0, 0x80, 0x40},      {7, 1, 0, 0, 0x80, 0x80},         {8, 0, 0, 0x40, 0x80, 0x80},
    {9, 1, 0, 0, 0x80, 0x80},         {10, 1, 0, 0, 0x80, 0x80},        {11, 0, 0, 0x40, 0x80, 0x80},
    {12, 1, 0, 0, 0x80, 0x80},        {13, 1, 0, 0, 0x80, 0x80},        {14, 0, 0, 0x40, 0x80, 0x80},
    {15, 3, 0, 0, 0x80, 0x80},        {16, 0, 0x40, 0, 0x80, 0x40},     {17, 0, 0x40, 0, 0x80, 0x40},
    {18, 0, 0, 0, 0x40, 0x40},        {19, 2, 0, 0x40, 0x40, 0x80},     {20, 2, 0x20, 0x20, 0x40, 0x40},
    {21, 2, 0, 0, 0x20, 0x20},        {22, 2, 0x20, 0, 0x40, 0x20},     {23, 2, 0x20, 0, 0x40, 0x20},
    {24, 2, 0x20, 0, 0x40, 0x20},     {25, 2, 0x40, 0x40, 0x80, 0x80}};

// 803E06F0 SimModel::s_SimPartsMapNames
const char* const SimModel::s_SimPartsMapNames[eSimPartsMapLocation::kCount] = {"ul", "ur", "ll", "lr"};

// 800730F4
int SimModel::UsingHighResolutionModel() { return s_forceLowResSims ? 0 : m_highRes; }

namespace {
// SimModel::Update* helpers: is the part's model there.
bool Has(const SimModel* m, int part) { return m->m_parts[part] && m->m_parts[part]->m_model; }
// The Update* functions' MorphUtilities calls: the head shape, and a body slider (0 body, 1 arms,
// 3 legs) by the description's value.
void MorphHead(SimModel* m) { MorphUtilities(m->m_parts).ApplyMorphHead(m->m_desc->m_morphHead); }
void MorphBody(SimModel* m, int what) {
  const CasSimDescriptionS2C* d = m->m_desc;
  MorphUtilities(m->m_parts).ApplyMorph(what, what == 0 ? d->m_morphBody : what == 1 ? d->m_morphArms : d->m_morphLegs);
}
}  // namespace

// 80073114
void SimModel::UpdateInnerLayerTorso(bool update, bool composite, bool morph) {
  bool updated = false;
  if (update) {
    if (!Has(this, eBodyPartS2C::kMidTorso)) {
      UpdateModelPart(eBodyPartS2C{7}, false, false);
      updated = true;
    } else {
      DeallocateModelPart(eBodyPartS2C{7});
    }
    if (updated && morph) { MorphHead(this); MorphBody(this, 0); }
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[7].m_location);
}

// 800731F8
void SimModel::UpdateInnerLayerSleeve(bool update, bool composite, bool morph) {
  bool updated = false;
  if (update) {
    if (!m_parts[eBodyPartS2C::kMidSleeve] && !m_parts[eBodyPartS2C::kOuterSleeve]) {
      UpdateModelPart(eBodyPartS2C{8}, false, false);
      updated = true;
    } else {
      DeallocateModelPart(eBodyPartS2C{8});
    }
    if (updated && morph) MorphBody(this, 1);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[8].m_location);
}

// 800732CC
void SimModel::UpdateMidLayerTorso(bool update, bool composite, bool morph, bool under) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{9}, false, false);
    if (under) {
      UpdateInnerLayerTorso(true, false, false);
      UpdateBelt(true, false, false);
    }
    if (morph) { MorphHead(this); MorphBody(this, 0); }
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[9].m_location);
}

// 800733A4
void SimModel::UpdateMidLayerSleeve(bool update, bool composite, bool morph, bool under) {
  bool updated = false;
  if (update) {
    if (!Has(this, eBodyPartS2C::kOuterSleeve)) {
      UpdateModelPart(eBodyPartS2C{11}, false, composite);
      updated = true;
      if (under) {
        UpdateInnerLayerSleeve(true, false, false);
        UpdateArmAccessory(true, false, false);
      }
    } else {
      DeallocateModelPart(eBodyPartS2C{11});
    }
    if (updated && morph) MorphBody(this, 1);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[11].m_location);
}

// 800734AC
void SimModel::UpdateMidLayerCollar(bool morph) {
  if (!Has(this, eBodyPartS2C::kOuterTorso)) {
    UpdateModelPart(eBodyPartS2C{10}, false, false);
    if (morph) MorphBody(this, 0);
  } else {
    DeallocateModelPart(eBodyPartS2C{10});
  }
}

// 80073544
void SimModel::UpdateOuterLayerTorso(bool update, bool composite, bool morph, bool under) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{12}, false, false);
    if (under) {
      UpdateMidLayerTorso(true, false, false, false);
      UpdateMidLayerCollar(false);
      UpdateInnerLayerTorso(true, false, false);
      UpdateBelt(true, false, false);
    }
    if (Has(this, eBodyPartS2C::kOuterTorso)) DeallocateModelPart(eBodyPartS2C{25});
    if (morph) { MorphHead(this); MorphBody(this, 0); }
  }
  if (composite) {
    if (OuterTorsoIsAHood()) CompositeSkinPart(2);
    CompositeSkinPart(s_SimPartsMap[12].m_location);
  }
}

// 80073698
void SimModel::UpdateOuterLayerSleeve(bool update, bool composite, bool morph, bool under) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{14}, false, false);
    if (under) {
      UpdateMidLayerSleeve(true, false, false, false);
      UpdateInnerLayerSleeve(true, false, false);
      UpdateArmAccessory(true, false, false);
    }
    if (morph) MorphBody(this, 1);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[14].m_location);
}

// 80073778
void SimModel::UpdateOuterLayerCollar(bool morph, bool composite) {
  UpdateModelPart(eBodyPartS2C{13}, false, false);
  if (morph) MorphBody(this, 0);
  if (composite && OuterCollarIsAHood()) CompositeSkinPart(2);
}

// 80073820
void SimModel::UpdateLowerBody(bool update, bool composite) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{15}, false, false);
    if (m_desc->m_lowerBodyOption == 0) DeallocateModelPart(eBodyPartS2C{17});
    else UpdateModelPart(eBodyPartS2C{17}, false, composite);
    MorphBody(this, 0);
    MorphBody(this, 3);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[15].m_location);
}

// 800738F0
// One of the hair parts by hair style, or the hair under a hat.
void SimModel::UpdateHair(bool update, bool composite, bool morph) {
  if (update) {
    if (m_desc->m_hairUnderHat == 0) {
      DeallocateModelPart(eBodyPartS2C{3});
      const int style = m_desc->m_hairStyle;
      for (int p = 2; p <= 6; ++p) {
        if (p == 3) continue;
        const int wanted = style == 0 ? 2 : style == 1 ? 4 : style == 2 ? 5 : style == 3 ? 6 : -1;
        if (p == wanted) UpdateModelPart(eBodyPartS2C{p}, false, false);
        else DeallocateModelPart(eBodyPartS2C{p});
      }
    } else {
      DeallocateModelPart(eBodyPartS2C{2});
      UpdateModelPart(eBodyPartS2C{3}, false, false);
      DeallocateModelPart(eBodyPartS2C{4});
      DeallocateModelPart(eBodyPartS2C{5});
      DeallocateModelPart(eBodyPartS2C{6});
    }
    if (morph) MorphHead(this);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[2].m_location);
}

// 80073ACC
void SimModel::UpdateHead(bool update, bool composite, bool morph) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{1}, false, false);
    if (morph) {
      MorphHead(this);
      MorphBody(this, 0);
      ApplyAllMorphTargets();
    }
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[19].m_location);
}

// 80073B78
void SimModel::UpdateHat(bool update, bool composite, bool morph) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{19}, false, false);
    UpdateHair(true, false, false);
    if (morph) MorphHead(this);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[19].m_location);
}

// 80073C1C
// Long sleeves hide the arm accessory unless it is one that shows over them.
void SimModel::UpdateArmAccessory(bool update, bool composite, bool morph) {
  if (update) {
    bool show = true;
    if (m_desc->m_midOption3 == 3 && Has(this, eBodyPartS2C::kMidSleeve) &&
        !m_simParts->LongSleeveDoesNotHideArmAccessory(eBodyPartS2C{11}, static_cast<unsigned int>(m_desc->m_midTop)))
      show = false;
    if (m_desc->m_outerOption3 == 3 && Has(this, eBodyPartS2C::kOuterSleeve) &&
        !m_simParts->LongSleeveDoesNotHideArmAccessory(eBodyPartS2C{14}, static_cast<unsigned int>(m_desc->m_outerTop)))
      show = false;
    if (show) {
      UpdateModelPart(eBodyPartS2C{21}, false, false);
      if (morph) MorphBody(this, 1);
    } else {
      DeallocateModelPart(eBodyPartS2C{21});
    }
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[21].m_location);
}

// 80073D60
// A vest or scarf replaces the outer layer.
void SimModel::UpdateVestScarf(bool update, bool composite, bool morph) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{25}, false, false);
    if (Has(this, eBodyPartS2C::kVestScarf)) {
      DeallocateModelPart(eBodyPartS2C{12});
      DeallocateModelPart(eBodyPartS2C{14});
      DeallocateModelPart(eBodyPartS2C{13});
    }
    if (morph) MorphBody(this, 0);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[25].m_location);
}

// 80073E38
void SimModel::UpdateNecklaceEarring(bool update, bool composite, bool morph) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{20}, false, false);
    if (morph) { MorphHead(this); MorphBody(this, 0); }
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[20].m_location);
}

// 80073EDC
// The belt unless a mid or outer top hides it; the music player unless the outer top hides it.
void SimModel::UpdateBelt(bool update, bool composite, bool morph) {
  if (update) {
    bool show = true;
    const unsigned int belt = static_cast<unsigned int>(m_desc->GetBodyPartIndex(eBodyPartS2C{eBodyPartS2C::kBelt}));
    if (Has(this, eBodyPartS2C::kMidTorso)) {
      const Sim__SimPartsS2CParam param = m_desc->GetBodyPartParam(eBodyPartS2C{eBodyPartS2C::kMidTorso});
      const unsigned int index = static_cast<unsigned int>(m_desc->GetBodyPartIndex(eBodyPartS2C{eBodyPartS2C::kMidTorso}));
      show = !m_simParts->MidLayerTorsoHidesBelt(index, static_cast<signed char>(param.m_option1), belt);
    }
    if (show && Has(this, eBodyPartS2C::kOuterTorso)) {
      const Sim__SimPartsS2CParam param = m_desc->GetBodyPartParam(eBodyPartS2C{eBodyPartS2C::kOuterTorso});
      const unsigned int index = static_cast<unsigned int>(m_desc->GetBodyPartIndex(eBodyPartS2C{eBodyPartS2C::kOuterTorso}));
      if (m_simParts->OuterLayerTorsoHidesBelt(index, static_cast<signed char>(param.m_option1), belt)) show = false;
    }
    if (show) UpdateModelPart(eBodyPartS2C{22}, false, false);
    else DeallocateModelPart(eBodyPartS2C{22});
    if (m_desc->m_beltExtra == 0) DeallocateModelPart(eBodyPartS2C{24});
    else UpdateModelPart(eBodyPartS2C{24}, false, false);
    if (m_desc->m_musicPlayer == 0) {
      DeallocateModelPart(eBodyPartS2C{23});
    } else {
      bool player = true;
      if (Has(this, eBodyPartS2C::kOuterTorso))
        player = !m_simParts->OuterLayerTorsoHidesMusicPlayer(static_cast<unsigned int>(m_desc->m_outerTop));
      if (player) UpdateModelPart(eBodyPartS2C{23}, false, false);
      else DeallocateModelPart(eBodyPartS2C{23});
    }
    if (morph) {
      if (m_desc->m_beltExtra) MorphHead(this);
      MorphBody(this, 0);
    }
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[22].m_location);
}

// 80074148
void SimModel::UpdateShoes(bool update, bool composite, bool morph) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{16}, false, false);
    if (m_desc->m_bootTop == 0) DeallocateModelPart(eBodyPartS2C{17});
    else UpdateModelPart(eBodyPartS2C{17}, false, false);
    if (morph && m_desc->m_bootTop) MorphBody(this, 3);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[16].m_location);
}

// 80074220
void SimModel::UpdateFacialFeature(bool update, bool composite, bool morph) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{18}, false, false);
    if (morph) MorphHead(this);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[18].m_location);
}

// 800742B0
void SimModel::UpdateGlasses(bool update, bool composite, bool morph) {
  if (update) {
    UpdateModelPart(eBodyPartS2C{0}, false, false);
    if (morph) MorphHead(this);
  }
  if (composite) CompositeSkinPart(s_SimPartsMap[0].m_location);
}


// ---------------------------------------------------------------------------------------------------
// SimRenderer

// 803D5730
const int SimRenderer::s_bodyPartRenderOrder[eBodyPartS2C::kCount] = {
    1, 8, 11, 14, 16, 17, 15, 7, 9, 10, 12, 13, 18, 20, 21, 22, 23, 24, 25, 0, 19, 2, 3, 4, 5, 6};

// 80074B30
// The parts in render order, skinned by `matrices` (the sim's node matrices); `shader` (if any) is
// selected first.
void SimRenderer::Render(ERC* rc, EIStaticModel** parts, EShader* shader, EMat4* matrices, unsigned int count,
                         bool morphHead) {
  if (shader) shader->Select(rc, 0);
  MorphParts(parts, morphHead);
  rc->ModelMatrixList(matrices, static_cast<int>(count));
  for (int part : s_bodyPartRenderOrder)
    if (EIStaticModel* m = parts[part]) m->DrawImmediate(rc, nullptr);
}

// 80074CA8
// Each part whose morph weights changed is morphed again (ERModel::ResetMorph, MorphLattices,
// MorphTargets, ApplyMorph); the head only when asked.
void SimRenderer::MorphParts(EIStaticModel** parts, bool morphHead) {
  for (int p = 0; p < eBodyPartS2C::kCount; ++p) {
    ERModel* m = parts[p] ? parts[p]->m_model : nullptr;
    if (!m || (p == 1 && !morphHead)) continue;
    m->ResetMorph();
    m->MorphLattices();
    m->MorphTargets();
    m->ApplyMorph();
  }
}
