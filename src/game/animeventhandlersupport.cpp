// Port of game/animeventhandlersupport.cpp: the frame-effect animation notes (kinds 0xD bloom, 0xE
// motion blur, 0xF depth of field), shared by the sims' animator (SAnimator2) and the camera director.
//
// A note's text is a row of the objects quickdata's FrameEffects*DataElement table, optionally
// followed by name:value pairs that override the row's fields ("stancelover:fadeInTime:0.5:
// fadeOutTime:0.5"); "disable" fades the effect out. The social stance animations (a2a_*_stanceer_*)
// and the story intro shots (cam_intro_lot*_story_part01) carry depth-of-field notes; motion blur
// and bloom notes in the shipped data name rows that are not in the tables (only "disable" acts).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
const char* const kFrameEffectDisableStr = "disable";  // 804FD920
const char kNoteValueSeparator = ':';                // 804FD924

// EString::Tokenize: the text split at the separator.
std::vector<std::string> Tokenize(const std::string& text) {
  std::vector<std::string> out;
  size_t pos = 0;
  while (true) {
    const size_t end = text.find(kNoteValueSeparator, pos);
    out.push_back(text.substr(pos, end == std::string::npos ? std::string::npos : end - pos));
    if (end == std::string::npos) break;
    pos = end + 1;
  }
  return out;
}

bool EqualsNoCase(const std::string& a, const char* b) {
  if (a.size() != std::strlen(b)) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
    if (x != y) return false;
  }
  return true;
}

const ERQuickdata* ObjectsQuickdata() { return Globs_pObjectFolder ? Globs_pObjectFolder->m_objects : nullptr; }

// The rows (FrameEffects*DataElement), as read from the quickdata.
struct MotionBlurRow {  // 0x14 bytes
  float colorOffset, previousFrameBlendAmount;
  int downSampleLevel;  // (the override stores a float's bits here, as the GameCube did)
  float fadeInTime, fadeOutTime;
};
struct BloomRow {  // 0x34 bytes
  float values[11];  // frameAmp, bloomThreshold, bloomAmp, bloomFrameBlends00..31
  float fadeInTime, fadeOutTime;
};
struct DOFRow {  // 0x1C bytes
  int downSampleLevel;
  float tint[3], blurFactor, fadeInTime, fadeOutTime;
};

// 800034C0
void OverrideMotionBlurSettingsFromTokens(MotionBlurRow& r, const std::vector<std::string>& t, int count) {
  for (int i = 1; i + 1 < count; i += 2) {
    const std::string& name = t[i];
    const char* value = t[i + 1].c_str();
    if (EqualsNoCase(name, "colorOffset")) r.colorOffset = static_cast<float>(std::atof(value));
    else if (EqualsNoCase(name, "previousFrameBlendAmount")) r.previousFrameBlendAmount = static_cast<float>(std::atof(value));
    else if (EqualsNoCase(name, "downSampleLevel")) {
      const float f = static_cast<float>(std::atoi(value));
      std::memcpy(&r.downSampleLevel, &f, 4);
    } else if (EqualsNoCase(name, "fadeInTime")) r.fadeInTime = static_cast<float>(std::atof(value));
    else if (EqualsNoCase(name, "fadeOutTime")) r.fadeOutTime = static_cast<float>(std::atof(value));
  }
}

// 800035D8
void OverrideBloomSettingsFromTokens(BloomRow& r, const std::vector<std::string>& t, int count) {
  static const char* const kNames[11] = {"frameAmp",           "bloomThreshold",     "bloomAmp",           "bloomFrameBlends00",
                                         "bloomFrameBlends01", "bloomFrameBlends10", "bloomFrameBlends11", "bloomFrameBlends20",
                                         "bloomFrameBlends21", "bloomFrameBlends30", "bloomFrameBlends31"};
  for (int i = 1; i + 1 < count; i += 2) {
    const std::string& name = t[i];
    const float value = static_cast<float>(std::atof(t[i + 1].c_str()));
    bool done = false;
    for (int k = 0; k < 11 && !done; ++k)
      if (EqualsNoCase(name, kNames[k])) r.values[k] = value, done = true;
    if (done) continue;
    if (EqualsNoCase(name, "fadeInTime")) r.fadeInTime = value;
    else if (EqualsNoCase(name, "fadeOutTime")) r.fadeOutTime = value;
  }
}

// 80003830
void OverrideDOFSettingsFromTokens(DOFRow& r, const std::vector<std::string>& t, int count) {
  for (int i = 1; i + 1 < count; i += 2) {
    const std::string& name = t[i];
    const char* value = t[i + 1].c_str();
    if (EqualsNoCase(name, "downSampleLevel")) r.downSampleLevel = std::atoi(value);
    else if (EqualsNoCase(name, "tintR")) r.tint[0] = static_cast<float>(std::atof(value));
    else if (EqualsNoCase(name, "tintG")) r.tint[1] = static_cast<float>(std::atof(value));
    else if (EqualsNoCase(name, "tintB")) r.tint[2] = static_cast<float>(std::atof(value));
    else if (EqualsNoCase(name, "blurFactor")) r.blurFactor = static_cast<float>(std::atof(value));
    else if (EqualsNoCase(name, "fadeInTime")) r.fadeInTime = static_cast<float>(std::atof(value));
    else if (EqualsNoCase(name, "fadeOutTime")) r.fadeOutTime = static_cast<float>(std::atof(value));
  }
}

// The note's row (its first token) in `table`, or null. `tokens` gets the tokens; the GameCube took
// the override pairs from the second token on while the token count minus one exceeded the index.
const void* FindRow(const ERQuickdata* qd, const char* table, const EAnimNote& note, std::vector<std::string>& tokens) {
  const void* t = qd->getTable(table);
  if (!t) return nullptr;
  tokens = Tokenize(note.m_text);
  if (tokens.empty()) return nullptr;
  return qd->getRow(t, tokens[0].c_str());
}
}  // namespace

// 80003998
void HandleBloomEvent(EAnimNote& note, FrameEffectsManager* fx) {
  if (!fx) return;
  const ERQuickdata* qd = ObjectsQuickdata();
  if (!qd || !qd->getTable("FrameEffectsBloomDataElement")) return;
  Bloom* bloom = fx->BloomObject();
  if (!bloom) return;
  if (EqualsNoCase(note.m_text, kFrameEffectDisableStr)) {
    bloom->Enable(false);
    return;
  }
  std::vector<std::string> tokens;
  const void* row = FindRow(qd, "FrameEffectsBloomDataElement", note, tokens);
  if (!row) return;
  BloomRow r;
  for (int i = 0; i < 11; ++i) r.values[i] = ERQuickdata::F32(row, i * 4);
  r.fadeInTime = ERQuickdata::F32(row, 0x2C);
  r.fadeOutTime = ERQuickdata::F32(row, 0x30);
  const int count = static_cast<int>(tokens.size());
  if (count > 1) {
    std::vector<std::string> pairs(tokens.begin() + 1, tokens.end());
    pairs.insert(pairs.begin(), std::string());  // keep the indices of the loop above (pairs from 1)
    OverrideBloomSettingsFromTokens(r, pairs, count);
  }
  bloom->m_fadeInTime = r.fadeInTime;
  bloom->m_fadeOutTime = r.fadeOutTime;
  BloomSettings s;
  s.frameAmp = r.values[0];
  s.bloomThreshold = r.values[1];
  s.bloomAmp = r.values[2];
  s.frameBlends[0][0] = r.values[3];
  s.frameBlends[0][1] = r.values[4];
  s.frameBlends[1][0] = r.values[5];
  s.frameBlends[1][1] = r.values[6];
  s.frameBlends[2][0] = r.values[7];
  s.frameBlends[2][1] = r.values[7];  // sic: the GameCube copied bloomFrameBlends20 here, not 21
  s.frameBlends[3][0] = r.values[9];
  s.frameBlends[3][1] = r.values[10];
  bloom->m_target = s;
  bloom->Enable(true);
}

// 80003C40
void HandleMotionBlurEvent(EAnimNote& note, FrameEffectsManager* fx) {
  if (!fx) return;
  const ERQuickdata* qd = ObjectsQuickdata();
  if (!qd || !qd->getTable("FrameEffectsMotionBlurDataElement")) return;
  MotionBlur* blur = fx->MotionBlurObject();
  if (!blur) return;
  if (EqualsNoCase(note.m_text, kFrameEffectDisableStr)) {
    blur->Enable(false);
    return;
  }
  std::vector<std::string> tokens;
  const void* row = FindRow(qd, "FrameEffectsMotionBlurDataElement", note, tokens);
  if (!row) return;
  MotionBlurRow r{ERQuickdata::F32(row, 0), ERQuickdata::F32(row, 4), ERQuickdata::S32(row, 8), ERQuickdata::F32(row, 0xC),
                  ERQuickdata::F32(row, 0x10)};
  const int count = static_cast<int>(tokens.size());
  if (count > 1) {
    std::vector<std::string> pairs(tokens.begin() + 1, tokens.end());
    pairs.insert(pairs.begin(), std::string());
    OverrideMotionBlurSettingsFromTokens(r, pairs, count);
  }
  blur->m_fadeInTime = r.fadeInTime;
  blur->m_fadeOutTime = r.fadeOutTime;
  blur->m_target.colorOffset = r.colorOffset;
  blur->m_target.downSampleLevel = r.downSampleLevel;
  blur->m_target.previousFrameBlend = r.previousFrameBlendAmount;
  blur->Enable(true);
}

// 80003DE0
void HandleDepthOfFieldEvent(EAnimNote& note, FrameEffectsManager* fx) {
  if (!fx) return;
  const ERQuickdata* qd = ObjectsQuickdata();
  if (!qd || !qd->getTable("FrameEffectsDepthOfFieldDataElement")) return;
  DepthOfField* dof = fx->DepthOfFieldObject();
  if (!dof) return;
  if (EqualsNoCase(note.m_text, kFrameEffectDisableStr)) {
    dof->Enable(false);
    return;
  }
  std::vector<std::string> tokens;
  const void* row = FindRow(qd, "FrameEffectsDepthOfFieldDataElement", note, tokens);
  if (!row) return;
  DOFRow r{ERQuickdata::S32(row, 0),
           {ERQuickdata::F32(row, 4), ERQuickdata::F32(row, 8), ERQuickdata::F32(row, 0xC)},
           ERQuickdata::F32(row, 0x10),
           ERQuickdata::F32(row, 0x14),
           ERQuickdata::F32(row, 0x18)};
  const int count = static_cast<int>(tokens.size());
  if (count > 1) {
    std::vector<std::string> pairs(tokens.begin() + 1, tokens.end());
    pairs.insert(pairs.begin(), std::string());
    OverrideDOFSettingsFromTokens(r, pairs, count);
  }
  dof->m_fadeInTime = r.fadeInTime;
  dof->m_fadeOutTime = r.fadeOutTime;
  dof->m_target.downSampleLevel = r.downSampleLevel;
  dof->m_target.tint[0] = r.tint[0];
  dof->m_target.tint[1] = r.tint[1];
  dof->m_target.tint[2] = r.tint[2];
  dof->m_target.blurFactor = r.blurFactor;
  dof->Enable(true);
}
