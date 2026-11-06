// Port of the pause menu's progress submenu, PRGTarget (game/targets_s2c.cpp). See prgtarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }

unsigned int PushFilterAll(int player, const char* name) {  // UIObjectBase::pushAptButtonFilterALL
  AptViewer* v = Viewer();
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}

void Copy16(unsigned short* dst, const unsigned short* src) {
  while (src && *src) *dst++ = *src++;
  *dst = 0;
}
void CopyAscii(unsigned short* dst, const char* src) {  // UTF8ToUCS2 for ASCII
  while (*src) *dst++ = static_cast<unsigned char>(*src++);
  *dst = 0;
}

// The player family (DAT_80475C5C, GameData +0xD8) and the story family 1 (the unlocks are kept there).
PlayerFamilyImpl* PlayerFamily(int number) {
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  return hood ? dynamic_cast<PlayerFamilyImpl*>(hood->GetFamily(number)) : nullptr;
}
// PlayerFamilyImpl::GetStatValue (8012D140, vtable 0x154): the family's stat (+0xCA8).
int FamilyStat(PlayerFamilyImpl* f, int index) { return f && index >= 0 && index < 0x19 ? f->m_stats[index] : 0; }
// GoalUnlock::GetUnlockTotal (800B8BF4) of family 1's goals and unlocks (vtable 0x14C): how many of a
// kind (0 recipes, 1 fashions, 2 lots, 5 objects) are unlocked.
int UnlockTotal(int kind) { return FamilyGoalUnlock(1).GetUnlockTotal(IGoalUnlock__UnlockType{kind}); }
// Player 1's primary neighbour's person data (GC: its person, Neighbor +0x18, vtable 0x13C; the
// person copies the neighbour's data).
int PrimaryPersonData(int field) {
  Neighbor* n = _gd.m_players[0].GetPrimaryNeighbor();
  return n && field >= 0 && field < 0x50 ? n->m_pers[field] : 0;
}
const int kSkillFields[7] = {10, 0xc, 0xb, 0x11, 0x12, 0xf, 9};
}  // namespace

// 801D8768
// The texts, the cancel sound, the screen (input for the pause player) and every pad command for the
// screen.
PRGTarget::PRGTarget(int player) : m_player(player) {
  AttachTables("PRGTarget");
  m_getTable->InstallCommand("PRG_exists", [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"PRG_CANCEL_BUTTON", "PRG_Close_Screen", "PRG_Cancel"}) m_setTable->InstallCommand(n, set, this);
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"PRG_title", "PRG_aspiration_title", "PRG_aspiration_value", "PRG_overall_title", "PRG_overall_value"})
    m_locTable->InstallCommand(n, loc, this);
  for (int i = 0; i < 9; ++i)
    for (const char* f : {"_name", "_value", "_percentage"})
      m_locTable->InstallCommand(("PRG_item" + std::to_string(i) + f).c_str(), loc, this);
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("PRG_SndCancel"), 0x2aaff358, const_cast<char*>(m_name));
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x28, m_player);
  m_filters[0] = PushFilterAll(0, "PRGTarget");
  m_filters[1] = _gd.m_fieldEC ? PushFilterAll(1, "PRGTarget") : 0;  // DAT_80475C70 two players
}

// 801D9098
PRGTarget::~PRGTarget() {
  if (AptViewer* v = Viewer()) {
    if (m_filters[0]) v->PopAptButtonFilter(0, m_filters[0]);
    if (m_filters[1]) v->PopAptButtonFilter(1, m_filters[1]);
  }
  m_filters[0] = m_filters[1] = 0;
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x28);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801D9228
void PRGTarget::SetVariable(char* name, char* value) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("PRG_Close_Screen", name) || !std::strcmp("PRG_Cancel", name)) Shutdown();  // 804FE51C PRG_CLOSE_MENU
  else if (!std::strcmp("PRG_CANCEL_BUTTON", name)) OnCancelKeyPressed(name, value);
}

// 801D92D0
void PRGTarget::FormatFraction(unsigned short* out, int value, int max) {
  char buf[20];
  std::snprintf(buf, sizeof buf, "%d/%d", value > max ? max : value, max);
  CopyAscii(out, buf);
}

// 801D9330
void PRGTarget::FormatPercentage(unsigned short* out, int value, int max) {
  char buf[20];
  std::snprintf(buf, sizeof buf, "%d%%", ((value > max ? max : value) * 100) / max);  // DAT_803F4BD0
  CopyAscii(out, buf);
}

// 801D9398
int PRGTarget::GetUnlockedFashions() { return UnlockTotal(1); }
// 801D9408
int PRGTarget::GetUnlockedRecipies() { return UnlockTotal(0); }
// 801D9478
int PRGTarget::GetUnlockedObjects() { return UnlockTotal(5); }

// 801D94E8
// The player family's promotions in the ten careers (stats 0xB..0x14), at most 90.
int PRGTarget::GetTotalPromotions() {
  PlayerFamilyImpl* f = PlayerFamily(_gd.m_fieldD8);
  int total = 0;
  for (int i = 0; i < 10; ++i) {
    total += FamilyStat(f, i + 0xb);
    if (total > 0x5a) total = 0x5a;
  }
  return total;
}

// 801D9578
// The careers at their top level (9).
int PRGTarget::GetMaxedCareers() {
  PlayerFamilyImpl* f = PlayerFamily(_gd.m_fieldD8);
  int n = 0;
  for (int i = 0; i < 10; ++i)
    if (FamilyStat(f, i + 0xb) == 9) ++n;
  return n;
}

// 801D9604
// Player 1's sim's seven skills, in levels (points / 100).
int PRGTarget::GetTotalSkillPoints() {
  int total = 0;
  for (int field : kSkillFields) total += static_cast<short>(PrimaryPersonData(field) / 100);
  return total;
}

// 801D97DC
int PRGTarget::GetMaxedSkills() {
  int n = 0;
  for (int field : kSkillFields) n += PrimaryPersonData(field) == 1000;
  return n;
}

// 801D9944
int PRGTarget::GetNPCsHelped() { return FamilyStat(PlayerFamily(_gd.m_fieldD8), 10); }

// 801D99A0
int PRGTarget::GetUnlockedLots() { return UnlockTotal(2); }

// 801D9A10
// The PDA's titles, family 1's aspiration points (as money, no symbol), each row's name, "n/max" and
// percentage (maxima 76, 24, 50, 90, 10, 70, 7, 15, 12) and the overall percentage (the nine
// percentages over 900).
unsigned short* PRGTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  const unsigned short* text = nullptr;
  bool copy = true;
  auto pda = [](const char* key) {
    const unsigned short* const* h = _globals.GetPDAString(key);
    return h ? *h : nullptr;
  };
  struct Row {
    const char* title;
    int (PRGTarget::*value)();
    int max;
  };
  static const Row kRows[9] = {{"unlock_fashion", &PRGTarget::GetUnlockedFashions, 0x4c},
                               {"unlock_recipes", &PRGTarget::GetUnlockedRecipies, 0x18},
                               {"unlock_objects", &PRGTarget::GetUnlockedObjects, 0x32},
                               {"unlock_promotions", &PRGTarget::GetTotalPromotions, 0x5a},
                               {"unlock_careers", &PRGTarget::GetMaxedCareers, 10},
                               {"unlock_skill_points", &PRGTarget::GetTotalSkillPoints, 0x46},
                               {"unlock_skill_maxed", &PRGTarget::GetMaxedSkills, 7},
                               {"unlock_wants_gold", &PRGTarget::GetNPCsHelped, 0xf},
                               {"unlock_locations", &PRGTarget::GetUnlockedLots, 0xc}};
  if (!std::strcmp("PRG_Text", name)) {
    CopyAscii(s, "Progress");
    copy = false;
  } else if (!std::strcmp("PRG_title", name)) {
    text = pda("header_progress");
  } else if (!std::strcmp("PRG_aspiration_title", name)) {
    text = pda("unlock_aspiration");
  } else if (!std::strcmp("PRG_aspiration_value", name)) {
    // Family 1's aspiration points (PlayerFamilyImpl::GetTotalAspirationScore, vtable 0x16C).
    PlayerFamilyImpl* f = PlayerFamily(1);
    StackString2<256> money;
    GetMoneyString(f ? f->m_totalAspirationScore : 0, money, false);
    Copy16(s, money.c_str());
    copy = false;
  } else if (!std::strcmp("PRG_overall_title", name)) {
    text = pda("unlock_overall");
  } else if (!std::strcmp("PRG_overall_value", name)) {
    int total = 0;
    for (const Row& r : kRows) {
      int v = (this->*r.value)();
      // (objects: twice the count; careers: ten times; the others value * 100 / max), each at most 100
      int pct = r.max == 0x32 ? v * 2 : r.max == 10 ? v * 10 : (v * 100) / r.max;
      total += pct < 100 ? pct : 100;
    }
    FormatPercentage(s, total, 900);
    copy = false;
  } else if (!std::strncmp("PRG_item", name, 8) && std::isdigit(static_cast<unsigned char>(name[8]))) {
    const int row = name[8] - '0';
    const char* field = name + 9;
    if (row < 9) {
      const Row& r = kRows[row];
      if (!std::strcmp("_name", field)) {
        text = pda(r.title);
      } else if (!std::strcmp("_value", field)) {
        FormatFraction(s, (this->*r.value)(), r.max);
        copy = false;
      } else if (!std::strcmp("_percentage", field)) {
        FormatPercentage(s, (this->*r.value)(), r.max);
        copy = false;
      }
    }
  } else if (!std::strcmp("PRG_HelpButton", name)) {
    const unsigned short* const* h = _globals.GetNewUIString("callout_help");
    Copy16(s, h ? *h : nullptr);
    copy = false;
  }
  if (copy) Copy16(s, text);
  UIQDTarget::UIQDSubstitutionStrings(s);
  std::size_t n = 0;
  while (s[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}

// 801DA538
char* PRGTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("PRG_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801DA5D4
void PRGTarget::OnCancelKeyPressed(char*, char*) {
  UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("Pause"));  // DAT_80475EF8
}

// 801DA620
void PRGTarget::UpdateShaders() {}

// 801DA624
void PRGTarget::Shutdown() {
  UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("Pause"));
}
