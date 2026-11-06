// Port of game/edithdialogprim.cpp (EdithDialogPrimitive: the behaviour trees' dialogs) and the dialog
// part of cXObjectImpl::ParseUIString (game/objectsim.cpp, 800FC7DC). See edithdialog.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

void GetMoneyString(int amount, StackString2<256>& out, bool symbol);  // wstringutil.cpp

namespace {
std::vector<unsigned short> Text(const std::u16string& s) {
  std::vector<unsigned short> v(s.begin(), s.end());
  v.push_back(0);
  return v;
}
std::u16string Str(const std::vector<unsigned short>& v) {
  std::u16string s;
  for (unsigned short c : v) {
    if (!c) break;
    s += static_cast<char16_t>(c);
  }
  return s;
}
std::u16string Str(const unsigned short* t) {
  std::u16string s;
  for (; t && *t; ++t) s += static_cast<char16_t>(*t);
  return s;
}
std::u16string Ascii(const char* a) {
  std::u16string s;
  for (; *a; ++a) s += static_cast<char16_t>(*a);
  return s;
}
std::string Narrow(const std::u16string& s) {
  std::string a;
  for (char16_t c : s) a += c < 0x80 ? static_cast<char>(c) : '?';
  return a;
}

// The STR# set `id` of an object file (ResFile +0x14: {strings, type, id} records).
const unsigned char* FileStringSet(const unsigned char* file, short id) {
  const auto* sets = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x14)) : nullptr;
  const unsigned int n = sets ? ERQuickdata::U32(sets - 4, 0) : 0;
  for (unsigned int i = 0; i < n; ++i)
    if (ERQuickdata::S16(sets + i * 12, 8) == id) return static_cast<const unsigned char*>(ERQuickdata::Ptr(sets + i * 12, 0));
  return nullptr;
}

// An object's name for a dialog: a person's first name, else its catalogue name.
std::u16string ObjectName(cXObject* o) {
  cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
  ObjSelector* sel = impl ? impl->m_selector : nullptr;
  if (!sel) return std::u16string();
  if (sel->GetIsPerson()) {
    if (impl->GetType() == 2)
      if (CasSimDescriptionS2C* d = static_cast<cXPersonImpl*>(impl)->GetSimDescription()) return Str(d->m_firstName);
    return sel->GetUserName();
  }
  return Str(*reinterpret_cast<const unsigned short* const*>(sel->GetCatalogName()));
}

// "$Token:N" (or "$Token:N:M" / "$Token:N,M") at or after `from`: its position, length and N, M (-1
// without) (ParseOneString 800FC604).
size_t FindToken(const std::u16string& s, const std::u16string& token, size_t from, size_t* len, int* n, int* m = nullptr) {
  const size_t at = s.find(token, from);
  if (at == std::u16string::npos) return at;
  size_t end = at + token.size();
  *n = -1;
  if (m) *m = -1;
  if (end < s.size() && s[end] == u':') {
    int v = 0;
    size_t k = end + 1;
    while (k < s.size() && s[k] >= u'0' && s[k] <= u'9') v = v * 10 + (s[k++] - u'0');
    if (k > end + 1) {
      *n = v;
      end = k;
      if (m && k + 1 < s.size() && (s[k] == u':' || s[k] == u',') && s[k + 1] >= u'0' && s[k + 1] <= u'9') {
        int w = 0;
        size_t j = k + 1;
        while (j < s.size() && s[j] >= u'0' && s[j] <= u'9') w = w * 10 + (s[j++] - u'0');
        *m = w;
        end = j;
      }
    }
  }
  *len = end - at;
  return at;
}
}  // namespace

// 800FC7DC cXObjectImpl::ParseUIString, the tokens dialogs use: $Neighbor (neighbour param 0's name),
// $Me (mine), $Object (the stack object's), $NameLocal:N / $LastNameLocal:N (local N's object),
// $Local:N, $MoneyLocal:N (local N's value), $PlayerFunds, $PlayerName:N (player N's primary sim).
// $LocationName:N ("HouseData" row local N's name), $JobName:N:M / $JobDesc:N:M (career local N's job
// local M: its name, the female one for a woman, / description), $JobTrackName:N (career local N's
// name), $TimeLocal:N (local N as an hour), $FoodName (my temp 0's recipe, temp 1 the variant).
void ParseDialogString(std::u16string& s, StackElem* frame, cXObjectImpl* me) {
  const short* locals = frame ? frame->GetLocals() : nullptr;
  const int numLocals = frame ? frame->m_numLocals : 0;
  auto local = [&](int n, short* v) {
    if (!locals || n < 0 || n >= numLocals) return false;
    *v = locals[n];
    return true;
  };
  auto each = [&](const char* token, auto&& with) {
    const std::u16string t = Ascii(token);
    size_t len = 0;
    int n = -1;
    for (size_t at = FindToken(s, t, 0, &len, &n); at != std::u16string::npos; at = FindToken(s, t, at + 1, &len, &n)) {
      std::u16string r;
      if (!with(n, r)) continue;
      s.replace(at, len, r);
    }
  };
  each("$NameLocal", [&](int n, std::u16string& r) {
    short id = 0;
    if (!local(n, &id)) return false;
    r = ObjectName(me->GetObjectFromID(id));
    return true;
  });
  each("$LastNameLocal", [&](int n, std::u16string& r) {
    short id = 0;
    if (!local(n, &id)) return false;
    cXObject* o = me->GetObjectFromID(id);
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    if (!impl || !impl->m_selector || !impl->m_selector->GetIsPerson()) return false;
    if (impl->GetType() == 2)
      if (CasSimDescriptionS2C* d = static_cast<cXPersonImpl*>(impl)->GetSimDescription()) r = Str(d->m_lastName);
    return true;
  });
  each("$MoneyLocal", [&](int n, std::u16string& r) {
    short v = 0;
    if (!local(n, &v)) return false;
    StackString2<256> m;
    GetMoneyString(v, m, false);  // MakeMoneyString (800FC5A0): no symbol (the text has its own $$)
    r = Str(m.c_str());
    return true;
  });
  each("$Local", [&](int n, std::u16string& r) {
    short v = 0;
    if (!local(n, &v)) return false;
    r = Ascii(std::to_string(v).c_str());
    return true;
  });
  each("$PlayerFunds", [&](int, std::u16string& r) {
    StackString2<256> m;
    GetMoneyString(Globs_pSimulator ? Globs_pSimulator->GetFunds() : 0, m, false);  // MakeMoneyString
    r = Str(m.c_str());
    return true;
  });
  each("$PlayerName", [&](int n, std::u16string& r) {
    if (n - 1u >= 2u) return false;
    Neighbor* nb = _gd.m_players[n - 1].GetPrimaryNeighbor();
    cXPersonImpl* p = nb ? NeighborPerson(nb) : nullptr;
    if (p) r = ObjectName(p);
    else if (nb) r = nb->m_selector ? nb->m_selector->GetUserName() : std::u16string();
    return true;
  });
  each("$Neighbor", [&](int, std::u16string& r) {
    const short* params = frame ? frame->GetParams() : nullptr;
    Neighbor* nb = params && frame->m_numParams > 0 && Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(params[0]) : nullptr;
    if (!nb || !nb->m_selector) return false;
    r = nb->m_selector->GetUserName();
    return true;
  });
  each("$Object", [&](int, std::u16string& r) {
    r = ObjectName(frame ? me->GetObjectFromID(frame->m_stackObject) : nullptr);
    return true;
  });
  each("$Me", [&](int, std::u16string& r) {
    r = ObjectName(me);
    return true;
  });
  each("$LocationName", [&](int n, std::u16string& r) {
    if (!frame || n < 0) return false;
    const void* table = _globals.m_strings ? _globals.m_strings->getTable("HouseData") : nullptr;
    if (!table || static_cast<unsigned int>(n) >= ERQuickdata::TableRowCount(table)) return false;
    r = Str(ERQuickdata::LocString(ERQuickdata::TableRow(table, static_cast<unsigned int>(n)), 0x14));
    return true;
  });
  // The career tokens: two locals (career id, level).
  auto careerEach = [&](const char* token, auto&& with) {
    const std::u16string t = Ascii(token);
    size_t len = 0;
    int n = -1, m = -1;
    for (size_t at = FindToken(s, t, 0, &len, &n, &m); at != std::u16string::npos; at = FindToken(s, t, at + 1, &len, &n, &m)) {
      short a = 0;
      if (!local(n, &a)) continue;
      const Career* c = Globs_pCareers ? static_cast<CareersImpl*>(Globs_pCareers)->GetCareerByID(a) : nullptr;
      if (!c) break;
      std::u16string r;
      if (!with(c, m, r)) continue;
      s.replace(at, len, r);
    }
  };
  careerEach("$JobName", [&](const Career* c, int m, std::u16string& r) {
    short level = 0;
    if (!local(m, &level) || level < 0 || level >= career::JobCount(c)) return false;
    bool male = true;
    if (me && me->GetType() == 2) male = static_cast<cXPersonImpl*>(me)->IsMale() != 0;
    r = Str(ERQuickdata::LocString(career::JobAt(c, level), male ? 0x58 : 0x5c));
    return true;
  });
  careerEach("$JobDesc", [&](const Career* c, int m, std::u16string& r) {
    short level = 0;
    if (!local(m, &level) || level < 0 || level >= career::JobCount(c)) return false;
    r = Str(ERQuickdata::LocString(career::JobAt(c, level), 0x6c));
    return true;
  });
  careerEach("$JobTrackName", [&](const Career* c, int, std::u16string& r) {
    r = Str(ERQuickdata::LocString(c, 8));
    return true;
  });
  each("$TimeLocal", [&](int n, std::u16string& r) {
    short v = 0;
    if (!local(n, &v)) return false;
    StackString2<256> t;
    GetTimeString(v, 0, nullptr, t);
    r = Str(t.c_str());
    return true;
  });
  // $FoodName: the recipe of my temp 0 (IngredientManager::GetFoodResult), its name for the variant in
  // temp 1 (FoodResult +0x24 + 4 * variant, 0..6; else the first); none: empty. Looked up once.
  bool first = true;
  std::u16string food;
  each("$FoodName", [&](int, std::u16string& r) {
    if (first) {
      first = false;
      IngredientManager* m = GetIngredientManagerInstance();
      const FoodResult* f = m && me ? m->GetFoodResult(me->m_temps[0]) : nullptr;
      if (f) {
        const int variant = me->m_temps[1];
        const unsigned int off = 0x24 + 4u * static_cast<unsigned int>(variant >= 1 && variant <= 6 ? variant : 0);
        if (const unsigned short* name = ERQuickdata::LocString(f, off)) food = Str(name);
      }
    }
    r = food;
    return true;
  });
}

EdithDialogPrimitive* TheEdithDialog() {
  static EdithDialogPrimitive* d = new EdithDialogPrimitive;
  return d;
}

// 800B51EC
EdithDialogPrimitive::EdithDialogPrimitive() {
  m_players = 3;
  m_onSelection = [this](int s) { SelectionCallback(s); };
  m_onZeroInput = [this] { ZeroInputCallback(); };
  m_onClosed = [this] { DialogClosedCallback(); };
}
// 800B5480
EdithDialogPrimitive::~EdithDialogPrimitive() = default;

// 800B5F20 EdithDialogPrimitive::GetModalDialogResult (inline in the header)

// 800B54B0
// The answer (-1: the last button) for the waiting TryDialog.
void EdithDialogPrimitive::SelectionCallback(int selection) {
  m_result = selection == -1 ? m_numSelections - 1 : selection;
  cXObjectImpl::overrideDialogReturnCode = 1;
}

// 800B54D4
void EdithDialogPrimitive::AssignString(const unsigned char* strings, std::vector<unsigned short>& out, int index,
                                        const unsigned short* fallback, bool) {
  if (index < 1) {
    if (fallback) out = Text(Str(fallback));
    return;
  }
  if (index == 0xfe) {
    out = Text(std::u16string());
    return;
  }
  const unsigned int count = strings ? ERQuickdata::U32(strings - 4, 0) : 0;
  if (static_cast<unsigned int>(index) <= count) {
    const std::u16string s = Str(ERQuickdata::LocString(strings, static_cast<unsigned int>(index - 1) * 4));
    if (!s.empty()) {
      out = Text(s);
      return;
    }
  }
  if (fallback) out = Text(Str(fallback));
}

// 800B55D0
void EdithDialogPrimitive::ResetDialogParameters() {
  m_body.clear();
  m_title.clear();
  for (auto& s : m_sel) s.clear();
  if (m_icon) {
    m_icon->DelRef();
    m_icon = nullptr;
  }
  m_timeoutMs = -1;
  m_iconH = 0;
  m_iconW = 0;
}

// 800B56DC
// GC: the strings come through the SmartDataFactory (STR# GUID 0x6BC37BC5, set 0x12D of the tree
// file's namespace); the texts go through the object's ParseUIString (vtable 0x10C).
int EdithDialogPrimitive::SetDialog(StackElem* frame, const unsigned char* param, cXObject* object, bool testing) {
  (void)testing;
  Behavior* b = frame ? frame->m_behavior : nullptr;
  const unsigned char* file = b ? b->GetResFileID(static_cast<short>((param[7] & 0x80) ? 0x2000 : 0x1000)) : nullptr;
  const unsigned char* strings = FileStringSet(file, 0x12d);
  cXObjectImpl* me = object ? object->GetObjectImplementation() : nullptr;
  auto parse = [&](std::vector<unsigned short>& t) {
    std::u16string s = Str(t);
    if (!s.empty() && me) ParseDialogString(s, frame, me);
    t = Text(s);
  };
  ResetDialogParameters();
  AssignString(strings, m_body, static_cast<signed char>(param[2]), nullptr, true);
  parse(m_body);
  AssignString(strings, m_title, static_cast<signed char>(param[6]), nullptr, true);
  parse(m_title);
  const int iconKind = param[7] >> 1 & 7;
  if (iconKind > 1) SIMMY_STUB("EdithDialogPrimitive::SetDialog: the icon (DialogIconTable, big thumbnails)", 0x800B56DC);
  const int kind = param[5] >> 4;
  if (std::getenv("SIMMY_DIALOG_TRACE") || true)
    std::printf("[dialog] kind %d: \"%s\" %s\n", kind, Narrow(Str(m_body)).c_str(), Narrow(Str(m_title)).c_str());
  // PC development aid: SIMMY_SHOT_ON_DIALOG=<text>: the --screenshot is taken 100 frames after the first
  // dialog whose text contains it (offscreen pictures of story moments).
  if (const char* shot = std::getenv("SIMMY_SHOT_ON_DIALOG")) {
    static bool taken = false;
    if (!taken && Narrow(Str(m_body)).find(shot) != std::string::npos) {
      taken = true;
      simmy::dev_options().screenshotFrame = static_cast<unsigned>(_framecount + 100);
      std::printf("[dialog] screenshot at frame %d (SIMMY_SHOT_ON_DIALOG)\n", _framecount + 100);
    }
  }
  if (kind == 1 || kind == 2 || kind == 5) {
    // Modeless: a hint at the screen's edge (2: for 60 s), or the intro thought balloon (10 s).
    AssignString(strings, m_sel[0], static_cast<signed char>(param[3]), nullptr, true);
    m_numSelections = 0;
    if (!Str(m_sel[0]).empty()) {
      parse(m_sel[0]);
      m_numSelections = 1;
    }
    m_preselect = 0;
    m_mode = 0;
    m_continueType = m_numSelections < 1 ? 1 : 0;
    m_field10 = 0;
    if (kind == 2) m_timeoutMs = 60000;
    if (kind == 5) m_timeoutMs = 10000;
    if (_gd.m_h2dTarget) {
      if (kind == 5) _gd.m_h2dTarget->SpawnIntroThoughtBalloon(this);
      else H2DTarget::SpawnModelessDialog(this);
    }
    ResetDialogParameters();
    return 1;
  }
  if (kind > 5) {
    m_numSelections = 0;
    m_body = Text(Ascii("WARNING: DEPRECATED DIALOG"));
    m_continueType = 1;
    if (_gd.m_h2dTarget) H2DTarget::SpawnModelessDialog(this);
    ResetDialogParameters();
    return 0;
  }
  // Modal: 3 one button, 4 yes/no, 0 by the buttons given.
  if ((param[5] & 0xf0) == 0x30) m_numSelections = 1;
  else if ((param[5] & 0xf0) == 0x40) m_numSelections = 2;
  else {
    m_numSelections = 1;
    if (static_cast<signed char>(param[4]) != -2) {
      m_numSelections = 2;
      if (static_cast<signed char>(param[0]) != -2) m_numSelections = 3;
    }
  }
  AssignString(strings, m_sel[0], static_cast<signed char>(param[3]),
               *_globals.GetNewUIString(m_numSelections == 1 ? "ok" : "yes"), true);
  parse(m_sel[0]);
  if (m_numSelections < 2) {
    m_sel[1].clear();
  } else {
    AssignString(strings, m_sel[1], static_cast<signed char>(param[4]), *_globals.GetNewUIString("no"), true);
    parse(m_sel[1]);
  }
  if (m_numSelections < 3) {
    m_sel[2].clear();
  } else {
    AssignString(strings, m_sel[2], static_cast<signed char>(param[0]), *_globals.GetNewUIString("cancel"), true);
    parse(m_sel[2]);
  }
  const int players = param[5] & 0xf;
  m_players = players == 1 || players == 2 ? players : 3;
  m_preselect = 0;
  m_mode = 0;
  m_continueType = m_numSelections < 1 ? 1 : 0;
  m_field10 = 0;
  m_field14 = 1;
  SpawnDialog();
  ResetDialogParameters();
  return 1;
}

// PC: the text of primitive 0x32 TryMakeActionString (objectsim.cpp): line `index` (0-based, GC
// StringSet::GetString) of the frame's private file's STR# set `set`, through ParseUIString.
std::u16string ActionStringText(StackElem* frame, int set, int index, cXObjectImpl* me) {
  Behavior* b = frame ? frame->m_behavior : nullptr;
  // The running tree's file (a semi-global tree's strings are its semi-global file's), else the private one.
  const short cls = frame && static_cast<unsigned short>(frame->GetTreeID()) >= 0x2000 ? 0x2000 : 0x1000;
  const unsigned char* strings = FileStringSet(b ? b->GetResFileID(cls) : nullptr, static_cast<short>(set));
  if (!strings && b) strings = FileStringSet(b->GetResFileID(cls == 0x2000 ? 0x1000 : 0x2000), static_cast<short>(set));
  const unsigned int count = strings ? ERQuickdata::U32(strings - 4, 0) : 0;
  std::u16string s;
  if (index >= 0 && static_cast<unsigned int>(index) < count) s = Str(ERQuickdata::LocString(strings, static_cast<unsigned int>(index) * 4));
  if (!s.empty() && me) ParseDialogString(s, frame, me);
  return s;
}
