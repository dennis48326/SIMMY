// Port of game/gamedata.cpp (partial).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/mp4.hpp"  // PC mod: 4 players

GameData _gd;  // 80475B84

// 8003D630
// The Sims 2 logo shader for the current language (the original returned the id in r3).
RET_UNKNOWN GameData::GetLocalizedSims2Logo() {
  switch (g_quickdatamanLanguage) {
  case 2: return 0xf2f00962;
  case 3: return 0x720fa61d;
  case 5: return 0xda2b0a7b;
  case 6: return 0xd3e8af20;
  default: return 0x41a71754;
  }
}

// 8003AFD4
// The memory meter (a debug window) and the two Players are not ported yet; the fields start at the
// values in the header.
GameData::GameData() = default;

// 8003B0FC
// The original deleted the objects at +0x54, +0x6C, +0x70 and +0x74 here; nothing creates them yet.
GameData::~GameData() = default;

// 8003D248
// The boot copyright notice: bit 1 fades it in (twice as fast as it fades out), bit 2 fades it out.
// Returns 1 on the frame a fade completes. Frame time is capped at 1/30 s so a slow load doesn't skip it.
int GameData::CopyrightUpdate(float dt) {
  const unsigned int flags = m_copyrightFlags;
  if (!(flags & 3)) return 0;
  int done = 0;
  if (dt > 1.0f / 30.0f) dt = 1.0f / 30.0f;
  float alpha = m_copyrightAlpha;
  if (flags & 1) {
    alpha += dt + dt;
    if (alpha >= 1.0f) {
      m_copyrightFlags = (flags & ~1u) | 4;
      done = 1;
      const float now = static_cast<float>(_pClockMan->GetSec(&_sysclock));
      m_copyrightTime = now;
      m_copyrightShownAt = now;
      alpha = 1.0f;
    }
  } else if (flags & 2) {
    alpha -= dt;
    if (alpha < 0.0f) {
      done = 1;
      m_copyrightFlags = (flags & ~2u) | 8;
      alpha = 0.0f;
    }
  }
  m_copyrightAlpha = alpha;
  return done;
}

// 8003D7C8
void GameData::SetBlackGraphicsBackgroundColor() {
  EVec3 black{0, 0, 0};
  _pGfx->SetBackgroundColor(black, 0xff, true);
}
// 8003D824
// Also black in this build (a debug build used a loud color to spot uncovered screen areas).
void GameData::SetUglyGraphicsBackgroundColor() {
  EVec3 black{0, 0, 0};
  _pGfx->SetBackgroundColor(black, 0xff, true);
}

namespace {
// GC: DVDGetDriveStatus() outside 4..6 and 0xB (cover open, no disc, wrong disc, retry): while the
// disc needs attention the game stops updating and drawing. The PC build has no disc.
bool DiscReady() { return true; }

// A mapped player's pad is unplugged when its status lacks the "connected" and "stable" bits (5). The
// player then takes over the first other pad that is in use (not the other player's).
void CheckPlayerPad(unsigned int player, unsigned int other, int& disconnected, int otherDisconnected) {
  if (!_pCtrlMan->GetPlayerMapped(player)) {
    disconnected = 0;
    return;
  }
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(player)));
  if ((pad->GetStatus() & 5) != 5) disconnected = 1;
  if (!disconnected) return;
  const int skip = (_pCtrlMan->GetPlayerMapped(other) && !otherDisconnected)
                       ? static_cast<signed char>(_pCtrlMan->GetPlayerControllerIndex(other))
                       : -1;
  for (int i = 0; i < 4; ++i) {
    if (i == skip || !_pCtrlMan->GetController(static_cast<unsigned int>(i))->IsInUse()) continue;
    if (Mp4PadOfExtraPlayer(i)) continue;  // PC mod: 4 players: an extra player's pad stays theirs
    disconnected = 0;
    _pCtrlMan->SwapPlayerControls(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(player)),
                                  static_cast<unsigned int>(i));
    break;
  }
}
}  // namespace

// 8003C798
// Before the state machines update: refresh the rumble state, handle unplugged pads, run the cheat
// listener. Returns 0 while the disc needs attention.
RET_UNKNOWN GameData::SystemPreUpdate() {
  if (_globals.m_vibrate) {
    _globals.m_vibrate->IsControllerReady(0);
    _globals.m_vibrate->IsControllerReady(1);
  }
  m_field2C = 0;
  CheckPlayerPad(0, 1, m_field30, m_field34);
  CheckPlayerPad(1, 0, m_field34, m_field30);
  if (!DiscReady()) return 0;
  if (_globals.m_cheats) SIMMY_STUB("GameData::SystemPreUpdate: ECheats::Update", 0x8003C798);
  return 1;
}

// 8003CE34
RET_UNKNOWN GameData::SystemPreDraw(ERC*) { return DiscReady() ? 1 : 0; }

// 8003CE78
void GameData::SystemPostUpdate() {}

// 8003CEC4
// A debug overlay (DAT_80476110, off in this build): darkens the area outside the TV-safe frame.
void GameData::SystemPostDraw(ERC*) {}

// 8003D6A0
// The EA logo movie for the current language; PAL and English share 0xbf28c91e.
RET_UNKNOWN GameData::GetLocalizedEAMovie() {
  switch (g_quickdatamanLanguage) {
  case 2: return 0x6fe48c50;
  case 3: return 0xbf28c91e;
  case 4: return 0x011f35aa;
  case 5: return 0xb65f0b68;
  case 6: return 0xc3b67b71;
  case 7: return 0xd96caf0c;
  case 8: return 0x283b9ecb;
  case 9: return 0xc43b6a81;
  case 10: return 0xe58145bc;
  default: return _iVideoMode != 1 ? 0xd5aec993 : 0xbf28c91e;
  }
}

// 8003D788
RET_UNKNOWN GameData::GetLocalizedSimsIntroMovie() { return _iVideoMode == 1 ? 0x2f109b75 : 0xc9761856; }

// 8003D7A8
RET_UNKNOWN GameData::GetLocalizedSimsEndingMovie() { return _iVideoMode == 1 ? 0xe593c635 : 0x96c85353; }

// 8003D344
// The boot copyright line in white, size 12, centred at (0.5, 0.875) with a 1-pixel drop shadow,
// faded by CopyrightUpdate.
void GameData::CopyrightDraw(ERC* rc) {
  ERFont* font = _globals.m_font;
  if (!(m_copyrightAlpha > 0.0f) || !font) return;
  EVec4 white{1, 1, 1, 1};
  font->SetColor(white);
  font->SetSize(12.0f, 1.0f, true);
  font->Select(rc);
  EVec2 pos{0.5f, 0.875f};
  font->DrawDs(rc, const_cast<unsigned short*>(EGlobal::NewUIText("copyright_string")), pos, EFontAlignX::Center,
               EFontAlignY::Top, nullptr, 1.0f, m_copyrightAlpha, true);
}

// 8003D138
void GameData::SetTwoPlayerMode(bool two) {
  UIDB::UIDBSetInt(const_cast<char*>("TwoPlayerGame"), two ? 1 : 0);
  m_fieldEC = two ? 1 : 0;  // DAT_80475C70
}

// 8003DDA0
int GameData::GetBuildBuyPlayer() const { return m_buildBuyPlayer; }
// 8003DDA8
// The player in build/buy mode (0 or 1; -1 none: anything else); the previous one is returned.
int GameData::SetBuildBuyPlayer(int player) {
  const int previous = m_buildBuyPlayer;
  m_buildBuyPlayer = player < 2 && player > -2 ? player : -1;
  return previous;
}

// ---------------------------------------------------------------------------------------------------
// Food creation / inventory screens (called by the object's UI primitive, objectsim.cpp's dialog 0x27)

// The refrigerator's level the food creation screen offers ingredients for (INGTarget::m_sRefrigeratorLevel).
int INGTarget_m_sRefrigeratorLevel = 0;

// 801B9458
// The level set; the old one.
int INGTarget::SetRefrigeratorLevel(int level) {
  const int old = INGTarget_m_sRefrigeratorLevel;
  INGTarget_m_sRefrigeratorLevel = level;
  return old;
}

// 8003DC3C
// The inventory / food creation screen `mode` (below 0xE) for player `player` (0/1) at lot `lot`: mode 0xB
// is the inventory's lot mode ("INV_Mode" 1), others 0; "ING_Mode" is 1 for mode 0xC, 2 for 0xD, else 0;
// modes 0xC and 0xD open food creation ("MixIngredients", with the refrigerator's level). 0: a bad
// player or mode, else 1.
RET_UNKNOWN GameData::GotoXamMode(int player, int mode, int lot, int fridgeLevel) {
  if (static_cast<unsigned int>(player) >= 2 || static_cast<unsigned int>(mode) >= 0xe) return 0;
  UIDB::UIDBSetInt(const_cast<char*>(player == 0 ? "INV_Mode" : "INV_ModeP2"), mode == 0xb ? 1 : 0);
  if (mode == 0xb) UIDB::UIDBSetInt(const_cast<char*>("INV_LotNumber"), lot);
  int ing = 0;
  if (mode == 0xc) ing = 1;
  else if (mode == 0xd) ing = 2;
  UIDB::UIDBSetInt(const_cast<char*>(player == 0 ? "ING_ModeP1" : "ING_ModeP2"), ing);
  UIDB::UIDBSetInt(const_cast<char*>("INV_LotNumber"), lot);
  if (mode >= 0xc) {
    UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), player);
    INGTarget::SetRefrigeratorLevel(fridgeLevel);
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MixIngredients"));
  }
  return 1;
}
