// Port of the pause menu's relationships submenu, R2LTarget (game/targets_s2c.cpp). See
// r2ltarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
void InstallShader(UIObjectBase* t, const char* name, unsigned int id) {
  UI2D::Instance()->InstallEntry(const_cast<char*>(name), id, const_cast<char*>(t->m_name), 0);
}
// The movie's copy of a localizable string (GC: SimsAptMalloc'd, after the UI string substitution).
unsigned short* FinishLocalizable(unsigned short* s) {
  UIQDTarget::UIQDSubstitutionStrings(s);
  std::size_t n = 0;
  while (s[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}
void Copy(unsigned short* dst, const unsigned short* src) {
  std::size_t n = 0;
  if (src)
    for (; src[n] && n < 0xfff; ++n) dst[n] = src[n];
  dst[n] = 0;
}
void Copy(unsigned short* dst, const std::vector<unsigned short>& src) {
  std::size_t n = 0;
  for (; n < src.size() && n < 0xfff; ++n) dst[n] = src[n];
  dst[n] = 0;
}
const unsigned short* PDAText(const char* name) {
  const unsigned short* const* slot = _globals.GetPDAString(name);
  return slot ? *slot : nullptr;
}

const char* const kIconNames[6] = {"R2L_Icon0", "R2L_Icon1", "R2L_Icon2", "R2L_Icon3", "R2L_Icon4", "R2L_Icon5"};
const char* const kIconFaceNames[6] = {"R2L_IconFace0", "R2L_IconFace1", "R2L_IconFace2",
                                       "R2L_IconFace3", "R2L_IconFace4", "R2L_IconFace5"};
const char* const kOldIconNames[6] = {"R2L_OldIcon0", "R2L_OldIcon1", "R2L_OldIcon2",
                                      "R2L_OldIcon3", "R2L_OldIcon4", "R2L_OldIcon5"};
const char* const kOldIconFaceNames[6] = {"R2L_OldIconFace0", "R2L_OldIconFace1", "R2L_OldIconFace2",
                                          "R2L_OldIconFace3", "R2L_OldIconFace4", "R2L_OldIconFace5"};
const char* const kIconTextNames[6] = {"R2L_IconText0", "R2L_IconText1", "R2L_IconText2",
                                       "R2L_IconText3", "R2L_IconText4", "R2L_IconText5"};
const char* const kOldIconTextNames[6] = {"R2L_OldIconText0", "R2L_OldIconText1", "R2L_OldIconText2",
                                          "R2L_OldIconText3", "R2L_OldIconText4", "R2L_OldIconText5"};
// R2L_IconFaceShaders (803F7BD4): the face for each relationship level.
const unsigned int kIconFaceShaders[12] = {0x9f93ff74, 0x9f93ff74, 0x6f4a7bfc, 0xdb74c55a, 0x816bcc00, 0x9f93ff74,
                                           0x7515fcf3, 0x7ffee5cc, 0x5812da26, 0x9b8d3041, 0xb7a62aad, 0x2e7f88d7};
constexpr unsigned int kBlank = 0x9f93ff74;
constexpr unsigned int kMarriageIcon = 0xd83770d4;
// g_useFlashInterface: 0 in this build (the reflows of the text sizes and the selector run).
constexpr int g_useFlashInterface = 0;

unsigned int FaceShader(int level) { return level >= 0 && level < 12 ? kIconFaceShaders[level] : kBlank; }

// The story sim's "RelationshipTable" row: +4 its icon, +8 its portrait, +0xC its description.
const void* StoryRow(unsigned int guid) {
  return reinterpret_cast<const void*>(_globals.GetRelationshipTableRow(guid));
}
}  // namespace

std::vector<RelatedPerson> PlayerRelatedPeople(int, cXPerson* person) {
  std::vector<RelatedPerson> out;
  Neighbor* me = PersonNeighbor(person);
  for (Neighbor* n : RelatedPeopleOf(person)) {
    RelatedPerson r;
    r.neighbor = n;
    me->GetRelationship(n, &r.value);
    r.married = me->IsMarriedTo(n) ? 1 : 0;
    r.level = me->CalculateRelationshipLevel(n);
    out.push_back(r);
  }
  return out;
}

// 801DA6F0
// The submenu for the pause menu's player: it takes every pad command, its variables and sounds, the
// screen (0x22), the relationships and the first page's shaders.
R2LTarget::R2LTarget(int player) : m_player(player) {
  AttachTables("R2LTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_filters[0] = PushFilterAll(0, "R2LTarget");
  m_filters[1] = _gd.m_fieldEC ? PushFilterAll(1, "R2LTarget") : 0;  // DAT_80475C70 two players
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"R2L_exists", "R2L_CancelEnabled", "R2L_ShowArrowL", "R2L_ShowArrowR", "R2L_ShowRelationshipIcon"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"R2L_Scroll", "R2L_Cancel", "R2L_ShowHelp", "R2L_CurrCell"}) m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"R2L_FullName", "R2L_Description", "R2L_RelationshipValue", "R2L_RelationshipLevel", "R2L_Title"})
    m_locTable->InstallCommand(n, loc, this);
  for (const char* n : kIconTextNames) m_locTable->InstallCommand(n, loc, this);
  for (const char* n : kOldIconTextNames) m_locTable->InstallCommand(n, loc, this);
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("R2L_SndBrowse"), 0x6a7a1517, const_cast<char*>(m_name));
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("R2L_SndCancel"), 0x76d81c2a, const_cast<char*>(m_name));
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x22, player);
  m_value = 0;
  m_page = 0;
  m_oldPage = -1;
  m_selected = -1;
  SetupRelationships();
  m_level = 4;
  SetupPageShaders();
}

// 801DAF90
R2LTarget::~R2LTarget() {
  m_records.clear();  // GC: each record's thumbnail and the portrait DelRef'd
  if (AptViewer* v = Viewer()) {
    if (m_filters[0]) v->PopAptButtonFilter(0, m_filters[0]);
    if (m_filters[1]) v->PopAptButtonFilter(1, m_filters[1]);
  }
  m_filters[0] = m_filters[1] = 0;
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x22);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801DB22C
// The movie's cell (on this page), help, scrolling a page left (1) or right (3), cancel back to the
// pause menu; the d-pad moves the cell or scrolls through the movie's own setters.
void R2LTarget::SetVariable(char* name, char* value) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  char buf[16];
  if (!std::strcmp("R2L_CurrCell", name)) {
    const unsigned int cell = static_cast<unsigned int>(std::atoi(value));
    if (cell < 6) ChangeSelection(m_page * 6 + static_cast<int>(cell));
  } else if (!std::strcmp("R2L_ShowHelp", name)) {
    SpawnSummaryDialog();
  } else if (!std::strcmp("R2L_Scroll", name)) {
    const int dir = std::atoi(value);
    if (dir == 1 && ScrollLeftEnabled()) {
      m_oldPage = m_page--;
      SetupPageShaders();
      ChangeSelection(m_selected - 6);
    } else if (dir == 3 && ScrollRightEnabled()) {
      m_oldPage = m_page++;
      SetupPageShaders();
      ChangeSelection(m_selected + 6);
    }
  } else if (!std::strcmp("R2L_Cancel", name)) {
    if (m_help.m_active || m_dialogActive) return;
    UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("Pause"));  // DAT_80475EF8
  } else if (!std::strcmp("R2L_DPAD_UP", name)) {
    const int cell = RecordIndexToGridIndex(m_selected);
    if (cell < 1) return;
    std::snprintf(buf, sizeof buf, "%i", cell - 1);
    SetVariable(const_cast<char*>("R2L_CurrCell"), buf);
  } else if (!std::strcmp("R2L_DPAD_DOWN", name)) {
    const int cell = RecordIndexToGridIndex(m_selected);
    if (cell < 0 || cell == 5) return;
    std::snprintf(buf, sizeof buf, "%i", cell + 1);
    SetVariable(const_cast<char*>("R2L_CurrCell"), buf);
  } else if (!std::strcmp("R2L_DPAD_LEFT", name)) {
    SetVariable(const_cast<char*>("R2L_Scroll"), const_cast<char*>("1"));
  } else if (!std::strcmp("R2L_DPAD_RIGHT", name)) {
    SetVariable(const_cast<char*>("R2L_Scroll"), const_cast<char*>("3"));
  }
}

// 801DB534
char* R2LTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  const int count = static_cast<int>(m_records.size());
  auto flag = [&](bool on) { std::strcpy(out, on ? "1" : "0"); };
  if (!std::strcmp("R2L_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  } else if (!std::strcmp("R2L_ShowRelationshipIcon", name)) {
    flag(m_selected >= 0 && m_selected < count);
  } else if (!std::strcmp("R2L_ShowArrowL", name)) {
    flag(ScrollLeftEnabled() != 0);
  } else if (!std::strcmp("R2L_ShowArrowR", name)) {
    flag(ScrollRightEnabled() != 0);
  } else if (!std::strcmp("R2L_CancelEnabled", name)) {
    std::snprintf(out, 0x20, "%d", !m_help.m_active && !m_dialogActive);
  }
  return out;
}

// 801DB6C4
// The selected relationship's name, description, value and level; a cell's value ("R2L_IconText<n>",
// the page scrolled from: "R2L_OldIconText<n>"); the title. No relationships: the "none" texts.
unsigned short* R2LTarget::GetLocalizable(char* name) {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  const int count = static_cast<int>(m_records.size());
  const bool selected = m_selected != -1 && m_selected <= count - 1;
  auto fail = [&]() -> unsigned short* {
    SimsAptFree(s);
    return nullptr;
  };
  if (!std::strcmp("R2L_FullName", name)) {
    if (!count) {
      Copy(s, PDAText("rel_none_name"));
    } else {
      if (!selected) return fail();
      std::vector<unsigned short> full = m_records[static_cast<size_t>(m_selected)].first;
      full.push_back(' ');
      const std::vector<unsigned short>& last = m_records[static_cast<size_t>(m_selected)].last;
      full.insert(full.end(), last.begin(), last.end());
      Copy(s, full);
    }
  } else if (!std::strcmp("R2L_Description", name)) {
    if (!count) Copy(s, PDAText("rel_none_desc"));
    else if (!selected) return fail();
    else Copy(s, m_description);
  } else if (!std::strcmp("R2L_RelationshipValue", name)) {
    if (selected) makeNumberString(m_value, s);
  } else if (!std::strcmp("R2L_RelationshipLevel", name)) {
    if (!selected) return fail();
    switch (m_level) {
      case 2: Copy(s, PDAText("rel_archenemy")); break;
      case 3: Copy(s, PDAText("rel_enemy")); break;
      case 4: Copy(s, PDAText("rel_neutral")); break;
      case 6: Copy(s, PDAText("rel_friend")); break;
      case 7: Copy(s, PDAText("rel_bestfriend")); break;
      case 8: Copy(s, PDAText("rel_crush")); break;
      case 9: Copy(s, PDAText("rel_lover")); break;
      default: break;
    }
  } else if (!std::strncmp("R2L_IconText", name, 12) || !std::strncmp("R2L_OldIconText", name, 15)) {
    int cell = 0;
    const bool old = name[4] == 'O';
    if (std::sscanf(name, old ? "R2L_OldIconText%d" : "R2L_IconText%d", &cell) != 1) return fail();
    const int record = old ? OldGridIndexToRecordIndex(cell) : GridIndexToRecordIndex(cell);
    if (record == -1) return fail();
    makeNumberString(m_records[static_cast<size_t>(record)].value, s);
  } else if (!std::strcmp("R2L_Title", name)) {
    Copy(s, PDAText("header_relationships"));
  }
  return FinishLocalizable(s);
}

// 801DBB98
void R2LTarget::Update() {}

// 801DBB9C
// The help's summary ("rel_help_title" / "rel_help_summary_body", one "ok" button); "ok" opens the
// full help ("rel_help").
void R2LTarget::SpawnSummaryDialog() {
  auto text = [](const unsigned short* const* handle) {
    std::vector<unsigned short> v;
    for (const unsigned short* s = handle ? *handle : nullptr; s && *s; ++s) v.push_back(*s);
    return v;
  };
  m_dialog.m_players = m_player == 0 ? 1 : 2;
  m_dialog.m_numSelections = 1;
  m_dialog.m_field1C = 1;
  m_dialog.m_sel[0] = text(_globals.GetNewUIString("ok"));  // DAT_803EF83C
  m_dialog.m_body = text(_globals.GetPDAString("rel_help_summary_body"));
  m_dialog.m_title = text(_globals.GetPDAString("rel_help_title"));
  m_dialog.m_sel[1].clear();
  m_dialog.m_sel[2].clear();
  m_dialog.m_mode = 3;
  m_dialog.m_field14 = 1;
  m_dialog.m_field24 = 0;
  m_dialog.m_onSelection = [this](int selection) { onSummaryDialogSelection(selection); };
  m_dialog.m_numSelections = 1;
  m_dialog.m_preselect = 0;
  m_dialog.m_field10 = 1;
  m_dialog.m_continueType = 0;
  m_dialog.m_onZeroInput = [this] { onSummaryDialogZeroInputCallback(); };
  m_dialog.m_onClosed = [this] { onSummaryDialogDialogClosedCallback(); };
  m_dialogActive = 1;
  m_dialog.SpawnDialog();
}

// 801DCE28
void R2LTarget::onHelpDialogSelection(int) {}

// 801DCE2C
// "ok": the full help.
void R2LTarget::onSummaryDialogSelection(int selection) {
  m_dialogActive = 0;
  if (selection == 0) m_help.SpawnHelpDialog(m_player, "rel_help", [this](int choice) { onHelpDialogSelection(choice); }, false, nullptr);
}

// 801DCE78
void R2LTarget::onSummaryDialogZeroInputCallback() { m_dialogActive = 0; }

// 801DCE84
void R2LTarget::onSummaryDialogDialogClosedCallback() { m_dialogActive = 0; }

// 801DA670
// qsort's order of two relationship records: by the first names (wcscmp), ties by the last names.
RET_UNKNOWN sort_records_by_name(void* a, void* b) {
  if (a == b) return 0;
  const R2LTarget::Record& ra = *static_cast<const R2LTarget::Record*>(a);
  const R2LTarget::Record& rb = *static_cast<const R2LTarget::Record*>(b);
  if (ra.first != rb.first) return ra.first < rb.first ? -1 : 1;
  if (ra.last != rb.last) return ra.last < rb.last ? -1 : 1;
  return 0;
}

// 801DBE40
// The selected sim's relationships (PlayerRelatedPeople), sorted by name, the first selected.
void R2LTarget::SetupRelationships() {
  m_records.clear();
  cXPerson* person = _globals.GetSelectedPerson(m_player);  // DAT_80475E88
  if (!person) return;
  for (const RelatedPerson& r : PlayerRelatedPeople(m_player, person)) {
    if (!r.neighbor) continue;
    Record rec;
    rec.guid = static_cast<unsigned int>(r.neighbor->GetGUID());
    rec.neighbor = r.neighbor;
    if (r.neighbor->m_selector) {
      const std::u16string& f = r.neighbor->m_selector->GetUserName();
      const std::u16string& l = r.neighbor->m_selector->GetUserLastName();
      rec.first.assign(f.begin(), f.end());
      rec.last.assign(l.begin(), l.end());
    }
    rec.value = r.value;
    rec.married = r.married;
    rec.level = r.level;
    m_records.push_back(std::move(rec));
  }
  // qsort with sort_records_by_name.
  std::sort(m_records.begin(), m_records.end(), [](const Record& a, const Record& b) {
    return sort_records_by_name(const_cast<Record*>(&a), const_cast<Record*>(&b)) < 0;
  });
  if (!m_records.empty()) ChangeSelection(0);
}

// 801DC1B4
// This page's and the page scrolled from's cells: each relationship's thumbnail (a story sim's icon)
// and the face of its level (married: the ring); then the cells' text sizes.
void R2LTarget::SetupPageShaders() {
  for (int i = 0; i < 6; ++i) {
    InstallShader(this, kIconNames[i], kBlank);
    InstallShader(this, kIconFaceNames[i], kBlank);
    InstallShader(this, kOldIconNames[i], kBlank);
    InstallShader(this, kOldIconFaceNames[i], kBlank);
  }
  if (m_records.empty()) return;
  const int count = static_cast<int>(m_records.size());
  auto cell = [&](const Record& r, const char* icon, const char* face, bool ring) {
    if (const void* row = StoryRow(r.guid)) InstallShader(this, icon, ERQuickdata::U32(row, 4));
    else InstallShader(this, icon, r.thumbnail ? r.thumbnail : kBlank);  // GC: ObjSelector::GetThumbnail
    InstallShader(this, face, ring && r.married ? kMarriageIcon : FaceShader(r.level));
  };
  for (int i = 0, rec = GridIndexToRecordIndex(0); i < 6 && rec >= 0 && rec < count; ++i, ++rec)
    cell(m_records[static_cast<size_t>(rec)], kIconNames[i], kIconFaceNames[i], true);
  for (int i = 0, rec = OldGridIndexToRecordIndex(0); i < 6 && rec >= 0 && rec < count; ++i, ++rec)
    cell(m_records[static_cast<size_t>(rec)], kOldIconNames[i], kOldIconFaceNames[i], false);
  if (!g_useFlashInterface) {
    m_reflow.OpenReflowBlock();
    for (int i = 0; i < 6; ++i) {
      m_reflow.Reflow_Id(kIconTextNames[i], 0);
      m_reflow.Reflow_FontSizeOverride(0xc);
      m_reflow.Reflow_Id(kOldIconTextNames[i], 0);
      m_reflow.Reflow_FontSizeOverride(0xc);
    }
    m_reflow.SubmitReflowBlock();
  }
}

// 801DC7A8
// The selected relationship: description (story sims), value, level icon (married: the ring), the
// large portrait; the texts refreshed and the selector moved to its cell.
void R2LTarget::ChangeSelection(int record) {
  m_selected = record;
  const int count = static_cast<int>(m_records.size());
  auto refresh = [&]() {
    if (g_useFlashInterface) return;
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id("R2L_FullName", 0);
    m_reflow.Reflow_FontSizeOverride(0x10);
    for (const char* id : {"R2L_Description", "R2L_RelationshipValue", "R2L_RelationshipLevel"}) {
      m_reflow.Reflow_Id(id, 0);
      m_reflow.Reflow_FontSizeOverride(0xe);
    }
    const int cell = RecordIndexToGridIndex(m_selected);
    m_reflow.Reflow_Id("R2L_SelectorIcon", 0);
    m_reflow.Reflow_X((cell / 6) * 0x28);
    m_reflow.Reflow_Y((cell % 6) * 0x28);
    m_reflow.SubmitReflowBlock();
  };
  if (record >= count || record < 0) {
    InstallShader(this, "R2L_RelationshipLevelIcon", kBlank);
    InstallShader(this, "R2L_RelationshipLevelMarriageIcon", kBlank);
    InstallShader(this, "R2L_Person2D", kBlank);
    refresh();
    return;
  }
  const Record& r = m_records[static_cast<size_t>(record)];
  const void* row = StoryRow(r.guid);
  m_description.clear();
  if (row)
    if (const unsigned short* d = ERQuickdata::LocString(row, 0xc))
      for (std::size_t i = 0; d[i]; ++i) m_description.push_back(d[i]);
  m_value = r.value;
  m_level = r.level;  // GC: CalculateRelationshipLevel against the player's selected neighbour
  if (m_level == -1) {
    InstallShader(this, "R2L_RelationshipLevelIcon", kBlank);
    InstallShader(this, "R2L_RelationshipLevelMarriageIcon", kBlank);
  } else {
    InstallShader(this, "R2L_RelationshipLevelIcon", FaceShader(m_level));
    InstallShader(this, "R2L_RelationshipLevelMarriageIcon", r.married ? kMarriageIcon : kBlank);
  }
  // The portrait: a story sim's, else the neighbour's big thumbnail (GC: ObjSelector::GetBigThumbnail,
  // not ported).
  InstallShader(this, "R2L_Person2D", row && ERQuickdata::U32(row, 8) ? ERQuickdata::U32(row, 8) : kBlank);
  refresh();
}

// 801DCD68
int R2LTarget::RecordIndexToGridIndex(int record) {
  const unsigned int cell = static_cast<unsigned int>(record - m_page * 6);
  return cell > 6 ? -1 : static_cast<int>(cell);
}
// 801DCD88
int R2LTarget::GridIndexToRecordIndex(int cell) {
  const int record = cell + m_page * 6;
  return record >= 0 && record < static_cast<int>(m_records.size()) ? record : -1;
}
// 801DCDB4
int R2LTarget::OldGridIndexToRecordIndex(int cell) {
  const int record = cell + m_oldPage * 6;
  return m_oldPage >= 0 && record >= 0 && record < static_cast<int>(m_records.size()) ? record : -1;
}
// 801DCDE8
int R2LTarget::ScrollLeftEnabled() { return m_page > 0 ? 1 : 0; }
// 801DCE00
int R2LTarget::ScrollRightEnabled() { return static_cast<int>(m_records.size()) - (m_page + 1) * 6 > 0 ? 1 : 0; }
