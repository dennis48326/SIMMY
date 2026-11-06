// Port of the pause menu's career submenu, CSPTarget (game/targets_s2c.cpp). See csptarget.hpp.
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
void InstallShader(CSPTarget* t, const char* name, unsigned int id) {
  if (t->m_installed) UI2D::Instance()->InstallEntry(const_cast<char*>(name), id, const_cast<char*>(t->m_name), 0);
}

constexpr unsigned int kBlank = 0x9f93ff74;  // get_blank_shader_id
// CSP_TabIconIds (803F6A14), CSP_TabSelectedIconIds (803F6A20).
const unsigned int kTabIconIds[3] = {0xa87ead52, 0xcf14d7a6, 0x50742f54};
const unsigned int kTabSelectedIconIds[3] = {0x6c2de84f, 0xa6572a1e, 0x81c087d2};
// CSP_CareerIdMap (803F6A2C) and CSP_CareerIconIds (803F6A58): career id -> icon.
const int kCareerIdMap[11] = {0, 0x17, 0x1a, 0x15, 0x16, 0x1c, 0x14, 0x1b, 0x12, 0x11, 0x13};
const unsigned int kCareerIconIds[11] = {0x9f93ff74, 0xcb3662b5, 0xd7672368, 0x8e69876c, 0x3242ecb6, 0xc4d4144c,
                                         0xf22615a1, 0xc9fe4685, 0x8dbdd8f6, 0xe6f217f4, 0xeaa3ca61};
// CSP_ZodiacIconIds (803F6A84): per sign 0..12.
const unsigned int kZodiacIconIds[13] = {0x9f93ff74, 0x9a05a3b4, 0xcd5119db, 0x6c19ce2f, 0x74bb10e6,
                                         0x41ef072e, 0xf9994ae4, 0x1fe2ada4, 0x3174c15f, 0x79e08c01,
                                         0xf44e7fcb, 0x8a739229, 0x14fd94c6};
// CSP_PromotionIconIds (803F6AB8): per requirement (0 friends, 1..7 the skills).
const unsigned int kPromotionIconIds[8] = {0x7b923e43, 0xcdd35218, 0xc7754b98, 0xb00eebb2,
                                           0x7c8d23c4, 0x2fe5b4ba, 0x0efcf589, 0x5cce0b69};
// CSP_PromotionIconNames (80437218).
const char* const kPromotionIconNames[3] = {"CSP_PromotionIcon0", "CSP_PromotionIcon1", "CSP_PromotionIcon2"};
const char* const kTabIconNames[3] = {"CSP_TabIcon0", "CSP_TabIcon1", "CSP_TabIcon2"};

std::u16string Str(const unsigned short* const* slot) {
  std::u16string s;
  if (slot && *slot)
    for (const unsigned short* p = *slot; *p; ++p) s.push_back(static_cast<char16_t>(*p));
  return s;
}
std::u16string Ascii(const char* a) {
  std::u16string s;
  while (*a) s.push_back(static_cast<char16_t>(static_cast<unsigned char>(*a++)));
  return s;
}
void Append(unsigned short* dst, const std::u16string& s) {
  unsigned short* d = dst;
  while (*d) ++d;
  for (char16_t c : s) *d++ = static_cast<unsigned short>(c);
  *d = 0;
}
void Append(unsigned short* dst, const unsigned short* const* slot) { Append(dst, Str(slot)); }
void AppendNumber(unsigned short* dst, int n) {  // make_number_string
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d", n);
  Append(dst, Ascii(buf));
}

// The person data the panel reads (GC: person vtable 0x13C).
int Data(cXPerson* p, int field) { return p ? p->GetPersonData(field) : 0; }
// GC: the object's vtable 0x204 (male). PC: the CAS description's gender.
bool IsFemale(cXPerson* p) {
  cXObject* o = p ? p->AsObject() : nullptr;
  auto* impl = o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
  return impl && impl->m_desc && !impl->m_desc->m_male;
}
}  // namespace

// 8003FE38 (game/eglobal.cpp)
// The name of want category `category`'s entry `id` (the UI strings' "WantCategory" table: per
// category its entries, 12 bytes: +0 the id, +4 the name), or the empty string.
RET_UNKNOWN EGlobal::GetWantFearCategoryString(int category, int id) {
  static const unsigned short kEmptyText[1] = {0};
  static const unsigned short* const kEmpty = kEmptyText;
  const void* table = m_strings ? m_strings->getTable(const_cast<char*>("WantCategory")) : nullptr;
  if (table) {
    const unsigned int n = ERQuickdata::TableRowCount(table);
    for (unsigned int i = 0; i < n; ++i) {
      const void* row = ERQuickdata::TableRow(table, i);
      if (ERQuickdata::S32(row, 0) != category) continue;
      const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 4));
      const unsigned int count = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
      for (unsigned int j = 0; j < count; ++j)
        if (ERQuickdata::U16(entries + j * 12, 0) == static_cast<unsigned int>(id))
          return reinterpret_cast<RET_UNKNOWN>(ERQuickdata::LocSlot(entries + j * 12, 4));
      break;
    }
  }
  return reinterpret_cast<RET_UNKNOWN>(&kEmpty);
}

// 801A7BC8
// The submenu for the pause menu's player: every pad command, the variables, the tab icons and sounds,
// the selected sim's skills, career and personality, the screen (0x23) and the first tab.
CSPTarget::CSPTarget(CSPTarget__eTabs tab, int player) {
  AttachTables("CSPTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_filters[0] = PushFilterAll(0, "CSPTarget");
  m_filters[1] = _gd.m_fieldEC ? PushFilterAll(1, "CSPTarget") : 0;  // DAT_80475C70 two players
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"CSP_exists", "CSP_CancelEnabled", "CSP_IsUnemployed"}) m_getTable->InstallCommand(n, get, this);
  for (int i = 0; i < 7; ++i) m_getTable->InstallCommand(("CSP_SkillValue" + std::to_string(i)).c_str(), get, this);
  for (int i = 0; i < 3; ++i) m_getTable->InstallCommand(("CSP_RequirementCurr" + std::to_string(i)).c_str(), get, this);
  for (int i = 0; i < 3; ++i) m_getTable->InstallCommand(("CSP_RequirementNext" + std::to_string(i)).c_str(), get, this);
  for (int i = 0; i < 5; ++i) m_getTable->InstallCommand(("CSP_ZodiacValue" + std::to_string(i)).c_str(), get, this);
  for (const char* n : {"CSP_Cancel", "CSP_CurrTab", "CSP_ShowHelp"}) m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"CSP_TabName", "CSP_Salary", "CSP_JobLevel", "CSP_Hours", "CSP_PromotionRequirements",
                        "CSP_CareerDescription", "CSP_CareerName", "CSP_CareerJobName", "CSP_CareerSalary",
                        "CSP_CareerHours", "CSP_PromotionFriends", "CSP_PromotionFriendsNext"})
    m_locTable->InstallCommand(n, loc, this);
  for (int i = 0; i < 7; ++i) m_locTable->InstallCommand(("CSP_SkillName" + std::to_string(i)).c_str(), loc, this);
  for (int i = 0; i < 7; ++i) m_locTable->InstallCommand(("CSP_SkillValueText" + std::to_string(i)).c_str(), loc, this);
  m_locTable->InstallCommand("CSP_ZodiacSignName", loc, this);
  for (const char* f : {"CSP_ZodiacName", "CSP_ZodiacOppositeName", "CSP_ZodiacValue"})
    for (int i = 0; i < 5; ++i) m_locTable->InstallCommand((f + std::to_string(i)).c_str(), loc, this);
  for (int i = 0; i < 3; ++i) InstallShader(this, kTabIconNames[i], kTabIconIds[i]);
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("CSP_SndTabChanged"), 0x649e20f6, const_cast<char*>(m_name));
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("CSP_SndCancel"), 0x76d81c2a, const_cast<char*>(m_name));
  m_tab = -1;
  m_player = player;
  if (cXPerson* person = _globals.GetSelectedPerson(player)) {
    SetupSkills(person);
    SetupCareer(person);
    SetupPersonality(person);
  }
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x23, m_player);
  ChangeSelectedTab(tab);
}

// 801A8A74
CSPTarget::~CSPTarget() {
  if (AptViewer* v = Viewer()) {
    if (m_filters[0]) v->PopAptButtonFilter(0, m_filters[0]);
    if (m_filters[1]) v->PopAptButtonFilter(1, m_filters[1]);
  }
  m_filters[0] = m_filters[1] = 0;
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x23);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801A8C70
// "CSP_Cancel" back to the pause menu (not while the help is up), "CSP_ShowHelp" the tab's help,
// "CSP_CurrTab" the tab the movie switched to.
void CSPTarget::SetVariable(char* name, char* value) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("CSP_Cancel", name)) {
    if (!m_help.m_active) UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("Pause"));
  } else if (!std::strcmp("CSP_ShowHelp", name)) {
    static const char* const kHelp[3] = {"career_help", "skill_help", "personality_help"};
    if (m_tab >= 0 && m_tab <= 2)
      m_help.SpawnHelpDialog(m_player, kHelp[m_tab], [this](int choice) { onHelpDialogSelection(choice); }, false, nullptr);
  } else if (!std::strcmp("CSP_CurrTab", name)) {
    ChangeSelectedTab(CSPTarget__eTabs{std::atoi(value)});
  }
}

// 801A8DC8
char* CSPTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  int i = -1;
  if (!std::strcmp("CSP_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
    return out;
  }
  if (!std::strcmp("CSP_IsUnemployed", name)) {
    std::strcpy(out, m_jobName.empty() ? "1" : "0");
  } else if (!std::strncmp("CSP_RequirementNext", name, 0x13)) {
    std::sscanf(name, "CSP_RequirementNext%i", &i);
    if (i >= 0 && i < 3 && m_reqSkill[i] != -1) std::snprintf(out, 0x20, "%i", m_reqNext[i]);
    else std::strcpy(out, "0");
  } else if (!std::strncmp("CSP_RequirementCurr", name, 0x13)) {
    std::sscanf(name, "CSP_RequirementCurr%i", &i);
    if (i >= 0 && i < 3 && m_reqSkill[i] != -1) std::snprintf(out, 0x20, "%i", m_reqCurr[i]);
    else std::strcpy(out, "0");
  } else if (!std::strncmp("CSP_SkillValue", name, 0xe)) {
    std::sscanf(name, "CSP_SkillValue%i", &i);
    std::snprintf(out, 0x20, "%i", i >= 0 && i < 7 ? m_skills[i] : 0);
  } else if (!std::strncmp("CSP_ZodiacValue", name, 0xf)) {
    std::sscanf(name, "CSP_ZodiacValue%i", &i);
    std::snprintf(out, 0x20, "%i", i >= 0 && i < 5 ? m_zodiacValues[i] : 0);
  } else if (!std::strcmp("CSP_CancelEnabled", name)) {
    std::snprintf(out, 0x20, "%d", m_help.m_active == 0);
  }
  return out;
}

// 801A9064
unsigned short* CSPTarget::GetLocalizable(char* name) {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  int i = -1;
  if (!std::strcmp("CSP_TabName", name)) {
    static const char* const kTabs[3] = {"header_career", "header_skills", "header_personality"};
    if (m_tab >= 0 && m_tab <= 2) Append(s, _globals.GetPDAString(kTabs[m_tab]));
  } else if (!std::strcmp("CSP_Salary", name)) {
    Append(s, _globals.GetPDAString("career_salary"));
  } else if (!std::strcmp("CSP_JobLevel", name)) {
    Append(s, _globals.GetPDAString("career_level"));
  } else if (!std::strcmp("CSP_Hours", name)) {
    Append(s, _globals.GetPDAString("career_hours"));
  } else if (!std::strcmp("CSP_PromotionRequirements", name)) {
    Append(s, Ascii("Promotion Requirements"));
  } else if (!std::strcmp("CSP_CareerDescription", name)) {
    Append(s, m_description);
  } else if (!std::strcmp("CSP_CareerName", name)) {
    Append(s, m_careerName);
  } else if (!std::strcmp("CSP_CareerJobName", name)) {
    AppendNumber(s, Data(_globals.GetSelectedPerson(m_player), 0x39) + 1);
    Append(s, Ascii(". "));
    Append(s, m_jobName);
  } else if (!std::strcmp("CSP_CareerSalary", name)) {
    if (m_salary != -1) {
      UIObjectBase::makeMoneyString(m_salary, s, true);
      Append(s, _globals.GetPDAString("career_rate"));
    }
  } else if (!std::strcmp("CSP_CareerHours", name)) {
    Append(s, m_hours);
  } else if (!std::strcmp("CSP_PromotionFriends", name)) {
    AppendNumber(s, m_friends);
  } else if (!std::strcmp("CSP_PromotionFriendsNext", name)) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "/ %i", m_friendsNext);
    Append(s, Ascii(buf));
  } else if (!std::strncmp("CSP_SkillName", name, 0xd)) {
    // The skills' want category 3 entries, in the panel's order.
    static const int kSkillIds[7] = {10, 0xc, 0xb, 0x11, 0x12, 0xf, 9};
    std::sscanf(name, "CSP_SkillName%i", &i);
    if (i >= 0 && i < 7)
      Append(s, reinterpret_cast<const unsigned short* const*>(_globals.GetWantFearCategoryString(3, kSkillIds[i])));
  } else if (!std::strncmp("CSP_SkillValueText", name, 0x12)) {
    std::sscanf(name, "CSP_SkillValueText%i", &i);
    if (i >= 0 && i < 7) AppendNumber(s, m_skills[i]);
  } else if (!std::strcmp("CSP_ZodiacSignName", name)) {
    Append(s, m_zodiacName);
  } else if (!std::strncmp("CSP_ZodiacOppositeName", name, 0x16)) {
    static const char* const kOpposite[5] = {"personality_2shy", "personality_5grouchy", "personality_4serious",
                                             "personality_1sloppy", "personality_3lazy"};
    std::sscanf(name, "CSP_ZodiacOppositeName%i", &i);
    if (i >= 0 && i < 5) Append(s, _globals.GetPDAString(kOpposite[i]));
  } else if (!std::strncmp("CSP_ZodiacName", name, 0xe)) {
    static const char* const kNames[5] = {"personality_2outgoing", "personality_5nice", "personality_4playful",
                                          "personality_1neat", "personality_3active"};
    std::sscanf(name, "CSP_ZodiacName%i", &i);
    if (i >= 0 && i < 5) Append(s, _globals.GetPDAString(kNames[i]));
  }
  UIQDTarget::UIQDSubstitutionStrings(s);
  std::size_t n = 0;
  while (s[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}

// 801A9918
void CSPTarget::Update() {}

// 801A991C
// The old tab's icon back to normal, the new one's lit.
void CSPTarget::ChangeSelectedTab(CSPTarget__eTabs tab) {
  const int old = m_tab;
  m_tab = tab;
  if (old >= 0 && old <= 2) InstallShader(this, kTabIconNames[old], kTabIconIds[old]);
  if (m_tab >= 0 && m_tab <= 2) InstallShader(this, kTabIconNames[m_tab], kTabSelectedIconIds[m_tab]);
}

// 801A9A64
// The career (person data 0x38) and its job at the sim's level (0x39), or "unemployed"; the next
// level's requirements: the friends (requirement 0) and the skills whose requirement is not 0.
void CSPTarget::SetupCareer(cXPerson* person) {
  auto* careers = static_cast<CareersImpl*>(Globs_pCareers);
  const Career* c = careers ? careers->GetCareerByID(Data(person, 0x38)) : nullptr;
  for (int i = 0; i < 3; ++i) m_reqSkill[i] = -1, m_reqNext[i] = 0, m_reqCurr[i] = 0;
  m_friends = 0;
  m_friendsNext = 0;
  if (!c || career::JobCount(c) == 0) {
    m_careerName = Str(_globals.GetPDAString("career_unemployed"));
    m_salary = -1;
    m_jobName.clear();
    m_hours.clear();
    InstallShader(this, "CSP_CareerIcon", kBlank);
    for (const char* n : kPromotionIconNames) InstallShader(this, n, kBlank);
    return;
  }
  int level = Data(person, 0x39);
  if (level < 0) level = 0;
  if (level > career::JobCount(c) - 1) level = career::JobCount(c) - 1;
  int icon = -1;
  for (int i = 0; i <= 10; ++i)
    if (kCareerIdMap[i] == career::Id(c)) {
      icon = i;
      break;
    }
  InstallShader(this, "CSP_CareerIcon", icon == -1 ? kBlank : kCareerIconIds[icon]);
  const Job* job = career::JobAt(c, level);
  m_description = Str(career::Description(job));
  m_careerName = Str(career::Name(c));
  m_jobName = Str(careers->GetJobName(job, IsFemale(person)));
  m_salary = career::Salary(job);
  // The hours: "h:00 - h:00" when the HUD's clock string is the 24-hour one, else "h AM - h PM".
  const std::u16string clock = Str(_globals.GetHUDUIString("time"));
  if (clock.find(u"hours24") != std::u16string::npos) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d:00 - %d:00", career::StartHour(job), career::EndHour(job));
    m_hours = Ascii(buf);
  } else {
    const int start = career::StartHour(job), end = career::EndHour(job);
    m_hours = Ascii(std::to_string(start % 12 == 0 ? 12 : start % 12).c_str());
    m_hours += start < 12 ? u" AM - " : u" PM - ";
    m_hours += Ascii(std::to_string(end % 12 == 0 ? 12 : end % 12).c_str());
    m_hours += end < 12 ? u" AM" : u" PM";
  }
  if (Globs_pNeighborhood)
    if (Neighbor* n = Globs_pNeighborhood->FindNeighborByID(Data(person, 0x1f)))  // GetNeighborID
      m_friends = static_cast<int>(n->GetFriendCount());
  int next = level + 1;
  if (next > career::JobCount(c) - 1) next = career::JobCount(c) - 1;
  const Job* nextJob = career::JobAt(c, next);
  int k = 0;
  for (int i = 0; i < 10; ++i) {
    if (i == 0) m_friendsNext = career::Requirement(nextJob, 0);
    if (career::Requirement(nextJob, i) != 0 && i != 0 && k < 3) {
      m_reqSkill[k] = i;
      m_reqNext[k] = career::Requirement(nextJob, i) / 100;
      ++k;
    }
  }
  InstallPromotionShaders();
}

// 801AA0A0
// Per requirement its skill's icon (and the sim's points in that skill), blank without one.
void CSPTarget::InstallPromotionShaders() {
  int icon = 0;
  for (int i = 0; i < 3; ++i) {
    const int skill = m_reqSkill[i];
    if (skill < 0) {
      InstallShader(this, kPromotionIconNames[i], kBlank);
      continue;
    }
    if (skill >= 1 && skill <= 7) {
      m_reqCurr[i] = m_skills[skill - 1];
      icon = skill;
    } else if (skill == 0) {
      icon = 0;
    }
    InstallShader(this, kPromotionIconNames[i], kPromotionIconIds[icon]);
  }
}

// 801AA208
// The seven skills (points / 100): person data 10, 0xC, 0xB, 0x11, 0x12, 0xF, 9.
void CSPTarget::SetupSkills(cXPerson* person) {
  m_skills[3] = Data(person, 0x11) / 100;
  m_skills[2] = Data(person, 0xb) / 100;
  m_skills[0] = Data(person, 10) / 100;
  m_skills[6] = Data(person, 9) / 100;
  m_skills[5] = Data(person, 0xf) / 100;
  m_skills[4] = Data(person, 0x12) / 100;
  m_skills[1] = Data(person, 0xc) / 100;
}

// 801AA38C
// The star sign from the person data (GC: its array, vtable 0x14C) and its icon; the five traits
// (person data 6, 2, 5, 7, 3: outgoing, nice, playful, neat, active) rounded to 0..10 as GC does.
void CSPTarget::SetupPersonality(cXPerson* person) {
  short pers[0x50] = {};
  for (int i = 0; i < 0x50; ++i) pers[i] = static_cast<short>(Data(person, i));
  m_zodiac = ComputeZodiacSign(pers);
  const unsigned short* sign = GetZodiacName(static_cast<short>(m_zodiac));
  m_zodiacName.clear();
  if (sign)
    for (const unsigned short* p = sign; *p; ++p) m_zodiacName.push_back(static_cast<char16_t>(*p));
  InstallShader(this, "CSP_ZodiacIcon",
                static_cast<unsigned int>(m_zodiac) < 13 ? kZodiacIconIds[m_zodiac] : kBlank);
  static const int kTraits[5] = {6, 2, 5, 7, 3};
  for (int i = 0; i < 5; ++i) {
    const int v = Data(person, kTraits[i]);
    m_zodiacValues[i] = v % 100 < 0x32 ? v / 100 : v / 0x65;
  }
}

// 801AA57C
void CSPTarget::onHelpDialogSelection(int) {}
