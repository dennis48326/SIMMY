// Port of the HUD's action queue widget ACTTarget (game/targets_s2c.cpp), the interest meter INTTarget
// (game/targets_tsc3.cpp) and the ActionQueue's HUD side (game/person.cpp: AddActionToHUD,
// RemoveActionFromHUD, FindVisualInsertGUID, DisableHUDAccess, EnableHUDAccess). See acttarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
int TwoPlayers() { return _gd.m_fieldEC != 0; }  // DAT_80475C70

std::size_t Len16(const unsigned short* s) {
  std::size_t n = 0;
  if (s)
    while (s[n]) ++n;
  return n;
}
// The movie's text: `text` with the UI substitutions, as a fresh SimsAptMalloc string.
unsigned short* Localizable(const std::u16string& text) {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  std::size_t i = 0;
  for (; i < text.size() && i < 0xfff; ++i) s[i] = static_cast<unsigned short>(text[i]);
  s[i] = 0;
  UIQDTarget::UIQDSubstitutionStrings(s);
  const std::size_t len = Len16(s);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((len + 1) * 2));
  std::memcpy(out, s, (len + 1) * 2);
  SimsAptFree(s);
  return out;
}
std::u16string Number(int n) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d", n);  // DAT_803F878C
  std::u16string s;
  for (const char* p = buf; *p; ++p) s += static_cast<char16_t>(*p);
  return s;
}
void PlaySound(unsigned int id) {
  if (UIAUDIO* audio = UIAUDIO::Instance()) audio->PlaySound(id);
}

cXPersonImpl* PersonImpl(cXPerson* person) {
  cXObject* o = person ? person->AsObject() : nullptr;
  return o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
}

// The record name tables (ACT_item0..17 etc. at 804FDE80..804FE0BC: "<player>ACT_item<n>").
struct RecordNames {
  std::string item[2][18], shader[2][18], frame[2][18], tween[2][18];
  RecordNames() {
    for (int p = 0; p < 2; ++p)
      for (int i = 0; i < 18; ++i) {
        const std::string pre = std::to_string(p), n = std::to_string(i);
        item[p][i] = pre + "ACT_item" + n;
        shader[p][i] = pre + "ACT_shdr" + n;
        frame[p][i] = pre + "ACT_frame" + n;
        tween[p][i] = pre + "ACT_item_tween" + n;
      }
  }
};
const RecordNames& Names() {
  static const RecordNames names;
  return names;
}

// GC: InteractorManager::GetPlayerInteractor(player, 0) != 0: the player's active interactor is the
// direct-control one (type 0); the action queue shows only in the classic control (SimInteractor, Z).
// SIMMY_ACT_SHOW=1 (development aid) shows the queue in direct control too.
bool InDirectControl(int player) {
  static const bool show = std::getenv("SIMMY_ACT_SHOW") && std::atoi(std::getenv("SIMMY_ACT_SHOW")) != 0;
  if (show) return false;
  InteractorModule__InteractorInfo info;
  return PlayerDirectInteractor(player) != nullptr && GetPlayerInteractorInfo(player, info);
}
bool InSocialMode(int player) {
  cXPersonImpl* p = PersonImpl(_globals.GetSelectedPerson(player));
  return p && p->IsInSocialMode() != 0;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// ACTTarget

// 801A0968
// The target ("<p>ACTTarget"), its variables, the 18 records' frames (shader 0xDD35680E) and names,
// the slots (0..6 from y 246 up in steps of 41; 7 and 8 to the side), then the player's screen.
ACTTarget::ACTTarget(int player) : m_player(player) {
  const std::string pre = std::to_string(player);
  AttachTables(player == 0 ? "0ACTTarget" : "1ACTTarget");  // 804FDE54 ACT_TargetName
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_getTable->InstallCommand((pre + "ACT_exists").c_str(), [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  m_setTable->InstallCommand((pre + "ACT_show_callback").c_str(), set, this);
  m_setTable->InstallCommand((pre + "ACT_hide_callback").c_str(), set, this);
  const RecordNames& names = Names();
  for (int i = 0; i < 18; ++i) m_setTable->InstallCommand(names.tween[player & 1][i].c_str(), set, this);
  for (int i = 0; i < 18; ++i)
    UI2D::Instance()->InstallEntry(const_cast<char*>(names.frame[player & 1][i].c_str()), 0xdd35680e,
                                   const_cast<char*>(m_name), 0);
  m_cancelMode = 0;
  m_cursor = m_lastCursor = nullptr;
  for (int i = 0; i < 7; ++i) m_slots[6 - i] = {0.0f, static_cast<float>(41 * i)};
  // GC: slots 7 and 8 are only moved 128 to the side (their y is never set).
  m_slots[7].x += 128.0f;
  m_slots[8].x += 128.0f;
  m_tail = nullptr;
  m_head = nullptr;
  for (int i = 0; i < 18; ++i) {
    ACTRecord& r = m_records[i];
    r.m_index = i;
    r.m_used = r.m_removing = 0;
    r.m_x = r.m_y = r.m_shownX = r.m_shownY = r.m_tweening = 0;
    r.m_next = r.m_prev = nullptr;
    r.m_item = names.item[player & 1][i].c_str();
    r.m_shader = names.shader[player & 1][i].c_str();
    r.m_frame = names.frame[player & 1][i].c_str();
    r.m_tween = names.tween[player & 1][i].c_str();
  }
  m_x = TwoPlayers() && m_player == 0 ? -0x140 : 0;
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(m_player == 0 ? 0x3c : 0x3d, 0);
}

// 801A19B0
ACTTarget::~ACTTarget() {
  for (iqRecord* r : m_pending) delete r;  // GC: the resource's reference with it
  m_pending.clear();
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(m_player == 0 ? 0x3c : 0x3d);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801A1C1C
// An item's tween ended (a removed one is free again); the widget's fade in or out ended.
void ACTTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strncmp("ACT_item_tween", name, 14)) {
    unsigned int n = 0;
    if (std::sscanf(name + 14, "%u", &n) == 1 && n < 18) {  // DAT_803EDDA0 "%d"
      ACTRecord& r = m_records[n];
      r.m_tweening = 0;
      if (r.m_removing) {
        r.m_used = 0;
        r.m_removing = 0;
      }
    }
  } else if (!std::strcmp("ACT_show_callback", name)) {
    m_tweening = 0;
  } else if (!std::strcmp("ACT_hide_callback", name)) {
    m_tweening = 0;
    UIReflow().ResetVisibility("ACT", false, m_player);
    m_shown = 0;
  }
}

// 801A1D3C
char* ACTTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("ACT_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801A1DD4
// No texts of its own.
unsigned short* ACTTarget::GetLocalizable(char*) { return Localizable(u""); }

// 801A1E58
// Shown while the interactors run, the player is not in direct control, the sim is not in social mode
// and the HUD is visible (faded in and out, sliding 22 to the side); then each pending addition is
// placed in a free record when it is still the selected sim's.
void ACTTarget::Update() {
  if (!m_exists) return;
  if (!PlayerDirectInteractor(0) && !PlayerDirectInteractor(1) && !PlayerSimInteractor(0)) {  // InteractorManager::IsInitialized
    HideActionQueue();
  } else {
    const bool direct = InDirectControl(m_player);
    const bool social = _globals.GetSelectedPerson(m_player) && InSocialMode(m_player);
    if (!m_shown) {
      if (!direct && !social) ShowActionQueue();
    } else if (direct || social) {
      HideActionQueue();
    }
  }
  if (m_wantShown && (!_gd.m_h2dTarget || _gd.m_h2dTarget->ShouldH2DBeVisible(m_player) != 1)) HideActionQueue();
  if (!m_tweening) {
    if (!m_wantShown) {
      if (m_shown) {
        UIReflow r;
        r.OpenTweenBlock("ACT", 0xfa, 0, "ACT_hide_callback", m_player);
        r.Tween_X(m_x + 0x16);
        r.Tween_Alpha(0);
        r.SubmitTweenBlock();
        m_tweening = 1;
      }
    } else if (!m_shown) {
      UIReflow r;
      r.ResetVisibility("ACT", true, m_player);
      r.OpenTweenBlock("ACT", 0xfa, 0, "ACT_show_callback", m_player);
      r.Tween_X(m_x);
      r.Tween_Alpha(0xff);
      r.SubmitTweenBlock();
      m_tweening = 1;
      m_shown = 1;
    }
  }
  while (!m_pending.empty()) {
    ACTRecord* record = GetRecord();
    if (!record) break;  // GC: never happens (18 records for at most 10 actions)
    iqRecord* q = m_pending.front();
    m_pending.pop_front();
    if (_globals.GetSelectedPerson(m_player) == q->person)
      PlaceItem(record, m_player, q->interaction, q->id, q->person, q->name, q->shader, q->resource, q->insertBefore);
    delete q;
  }
}

// 801A21EC
void ACTTarget::ShowActionQueue() { m_wantShown = 1; }
// 801A23BC
int ACTTarget::IsActionQueueActive() { return m_shown; }
// 801A2B34
int ACTTarget::IsActionQueueAvailable(int) const { return 1; }
// 801A21F8
void ACTTarget::HideActionQueue() { m_wantShown = 0; }

// 801A2204
// Each record that is not tweening and has somewhere new to go is tweened there (fading out over 125 ms
// when it goes or is being removed, else in over 250 ms); the cancel selector follows its item (or
// moves to the next one when its item went).
void ACTTarget::UpdateActionQueue() {
  for (ACTRecord& r : m_records) {
    if (r.m_tweening) continue;
    if (r.m_shownX == r.m_x && r.m_shownY == r.m_y) continue;
    r.m_shownX = r.m_x;
    r.m_shownY = r.m_y;
    UIReflow reflow;
    if (r.m_x == 0 && !r.m_removing) {
      reflow.OpenTweenBlock(r.m_item, 0xfa, 0, r.m_tween, m_player);
      reflow.Tween_Alpha(0xff);
    } else {
      reflow.OpenTweenBlock(r.m_item, 0x7d, 0, r.m_tween, m_player);
      reflow.Tween_Alpha(0);
      r.m_removing = 1;
    }
    reflow.Tween_X(r.m_x);
    reflow.Tween_Y(r.m_y);
    reflow.SubmitTweenBlock();
    r.m_tweening = 1;
    r.m_used = 1;
    if (m_cursor == &r) {
      if (!r.m_removing) {
        reflow.OpenTweenBlock("ACT_SelectorIcon", 0xfa, 0, "NO_CALLBACK", m_player);
        reflow.Tween_X(r.m_x + 0x20);
        reflow.Tween_Y(r.m_y - 0xe);
        reflow.SubmitTweenBlock();
      } else if (!r.m_next) {
        ExitCancelMode();
      } else {
        m_lastCursor = &r;
        m_cursor = r.m_next;
        UpdateCancelModeWidgets();
      }
    }
  }
}

// 801A23C4
// The selected sim's newly visible action waits for Update (once: not when it is pending or shown).
void ACTTarget::AddAction(int player, Interaction* interaction, const std::u16string& name, unsigned int shader,
                          ERShader* resource, unsigned int insertBefore) {
  if (!interaction || interaction->m_id == 0 || _globals.GetSelectedPerson(m_player) != interaction->GetPerson() ||
      m_player != player)
    return;
  for (const iqRecord* q : m_pending)
    if (q->id == interaction->m_id && q->person == interaction->GetPerson()) return;
  for (ACTRecord* r = m_head; r; r = r->m_next)
    if (r->m_id == interaction->m_id) return;
  auto* q = new iqRecord;
  q->person = interaction->GetPerson();
  q->interaction = interaction;
  q->id = interaction->m_id;
  q->name = name;
  q->shader = shader;
  q->resource = resource;  // GC: EResource::AddRef
  q->insertBefore = insertBefore;
  m_pending.push_back(q);
}

// 801A25B8
// The record shows the action: its icon (the object's shader), linked into the queue in front of the
// one with id `insertBefore` (else at the end), the queue's positions redone, and it slides in from
// 128 to the right (250 ms) with its frame untinted.
void ACTTarget::PlaceItem(ACTRecord* record, int, Interaction*, int id, cXPerson* person, const std::u16string&,
                          unsigned int shader, ERShader* resource, unsigned int insertBefore) {
  record->m_interaction = nullptr;  // GC: +0x3C the interaction pointer (looked up again by id when used)
  record->m_id = id;
  record->m_person = person;
  if (!resource) {
    UI2D::Instance()->InstallEntry(const_cast<char*>(record->m_shader), shader, const_cast<char*>(m_name), 0);
  } else {
    // A sim's portrait (or another ready-made shader).
    UI2D::Instance()->InstallEntry(const_cast<char*>(record->m_shader), resource, const_cast<char*>(m_name), 0);
  }
  bool linked = false;
  if (!m_head) {
    m_head = m_tail = record;
    record->m_prev = record->m_next = nullptr;
    linked = true;
  } else if (insertBefore) {
    for (ACTRecord* r = m_head; r && !linked; r = r->m_next) {
      if (static_cast<unsigned int>(r->m_id) != insertBefore) continue;
      record->m_next = r;
      record->m_prev = r->m_prev;
      if (!r->m_prev) m_head = record;
      else r->m_prev->m_next = record;
      r->m_prev = record;
      linked = true;
    }
  }
  if (!linked) {
    m_tail->m_next = record;
    record->m_prev = m_tail;
    m_tail = record;
    record->m_next = nullptr;
  }
  DebugListValidation();
  record->m_used = 1;
  record->m_removing = 0;
  record->m_slot = -1;
  ReOrderActionQueue();
  UIReflow reflow;
  reflow.OpenReflowBlock();
  reflow.Reflow_Id(record->m_item, m_player);
  record->m_shownY = record->m_y;
  record->m_shownX = record->m_x + 0x80;
  reflow.Reflow_X(record->m_shownX);
  reflow.Reflow_Y(record->m_shownY);
  reflow.Reflow_Alpha(0);
  reflow.Reflow_Visibility(true);
  reflow.Reflow_Id(record->m_frame, m_player);
  reflow.Reflow_ColorOnlyOverride(0xffffff);
  reflow.SubmitReflowBlock();
  reflow.OpenTweenBlock(record->m_item, 0xfa, 0, record->m_tween, m_player);
  reflow.Tween_X(record->m_x);
  reflow.Tween_Y(record->m_y);
  reflow.Tween_Alpha(0xff);
  reflow.SubmitTweenBlock();
  record->m_tweening = 1;
  record->m_shownY = record->m_y;
  record->m_shownX = record->m_x;
}

// 801A2828
// A pending addition is dropped; a shown one is unlinked (the cancel selector moves on), sent 128 to
// the right of its slot to fade out, and the others close up.
void ACTTarget::RemoveAction(int, Interaction* interaction) {
  if (!interaction) return;
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    iqRecord* q = *it;
    if (q->id == interaction->m_id && q->person == interaction->GetPerson()) {
      m_pending.erase(it);
      delete q;
      return;
    }
  }
  for (ACTRecord* r = m_head; r; r = r->m_next) {
    if (r->m_id != interaction->m_id || r->m_person != interaction->GetPerson()) continue;
    if (!r->m_prev) {
      m_head = r->m_next;
      if (!m_head) m_tail = nullptr;
      else m_head->m_prev = nullptr;
    } else {
      r->m_prev->m_next = r->m_next;
      if (r->m_next) r->m_next->m_prev = r->m_prev;
      if (r == m_tail) m_tail = r->m_prev;
    }
    if (m_cursor == r) {
      if (!r->m_next) {
        ExitCancelMode();
      } else {
        m_lastCursor = nullptr;
        m_cursor = r->m_next;
        UpdateCancelModeWidgets();
      }
    }
    DebugListValidation();
    const int slot = r->m_slot >= 0 && r->m_slot < 9 ? r->m_slot : 8;
    r->m_removing = 1;
    r->m_x = static_cast<int>(m_slots[slot].x) + 0x80;
    ReOrderActionQueue();
    return;
  }
}

// 801A2AAC
// Every shown record's action (found by id in the sim's queue) is removed, cancel mode ends.
void ACTTarget::RemoveAllActions(int player) {
  for (ACTRecord& r : m_records) {
    if (!r.m_used) continue;
    r.m_interaction = GetInteractionFromID(r.m_id);
    if (r.m_interaction) RemoveAction(player, r.m_interaction);
  }
  if (m_cursor) ExitCancelMode();
}

// 801A2B3C
ACTTarget::ACTRecord* ACTTarget::GetRecord() {
  for (ACTRecord& r : m_records)
    if (!r.m_used) return &r;
  return nullptr;
}

// 801A2BA4
// The queue's records that are not leaving take the slots in order (from the 9th on, all slot 8).
void ACTTarget::ReOrderActionQueue() {
  int slot = 0;
  for (ACTRecord* r = m_head; r; r = r->m_next) {
    if (r->m_removing) continue;
    if (r->m_slot != slot) {
      r->m_slot = slot;
      r->m_x = static_cast<int>(m_slots[slot].x);
      r->m_y = static_cast<int>(m_slots[slot].y);
    }
    if (++slot > 8) slot = 8;
  }
}

// 801A2C30
void ACTTarget::DebugListValidation() {}

// 801A2C34
// The selected sim's queued (or running) action with unique id `id`.
Interaction* ACTTarget::GetInteractionFromID(int id) {
  cXPersonImpl* person = PersonImpl(_globals.GetSelectedPerson(m_player));
  if (!person) return nullptr;
  const int n = person->CountActions(true);  // person vtable 0xDC
  for (int i = 0; i < n; ++i)
    if (Interaction* a = person->GetIndAction(i, true); a && a->m_id == id) return a;  // vtable 0xE4
  return nullptr;
}

// 801A2CDC
// GC: the record above's array index must be below 9 (its index, not its slot, as the original reads).
void ACTTarget::MoveCancelCursorUp() {
  if (m_cursor && m_cursor->m_next && m_cursor->m_next->m_index < 9) {
    m_lastCursor = m_cursor;
    m_cursor = m_cursor->m_next;
    UpdateCancelModeWidgets();
  }
}

// 801A2D30
void ACTTarget::MoveCancelCursorDown() {
  if (m_cursor && m_cursor->m_prev) {
    m_lastCursor = m_cursor;
    m_cursor = m_cursor->m_prev;
    UpdateCancelModeWidgets();
  }
}

// 801A2D78
// Cancel mode on (only with a queue shown and not empty: the selector on the head item) or off.
// Returns whether it is on.
int ACTTarget::CancelModeEnabled(bool on) {
  if ((on && !m_head) || !m_wantShown) return 0;
  m_cancelMode = on ? 1 : 0;
  if (on) {
    UIReflow().ResetVisibility("ACT_SelectorIcon", true, m_player);
    if (m_head) {
      m_cursor = m_head;
      m_lastCursor = nullptr;
      UpdateCancelModeWidgets();
    }
  } else {
    ExitCancelMode();
  }
  return on ? 1 : 0;
}

// 801A2E30
// A in cancel mode: the selected item's action is removed from the sim's queue (person vtable 0xBC,
// sound 0x374BDCEA) unless it is flagged 0x100 (sound 0x2AAFF358); the selector moves up (or down at
// the top), cancel mode ends when nothing is left.
void ACTTarget::CancelCurrentObject() {
  ACTRecord* r = m_cursor;
  if (!r || r->m_removing) return;
  r->m_interaction = GetInteractionFromID(r->m_id);
  if (!r->m_interaction) return;
  cXPersonImpl* person = PersonImpl(r->m_interaction->GetPerson());
  if (r->m_interaction->m_flags & Interaction::kCancelled) {
    PlaySound(0x2aaff358);
    return;
  }
  if (person) person->RemoveAction(r->m_id);
  PlaySound(0x374bdcea);
  std::printf("[act] player %d cancels queued action %d\n", m_player + 1, r->m_id);
  if (!m_cancelMode) return;
  if (r->m_next && r->m_next->m_index <= 8) {
    m_lastCursor = nullptr;
    m_cursor = r->m_next;
  } else if (r->m_prev) {
    m_lastCursor = nullptr;
    m_cursor = r->m_prev;
  } else {
    ExitCancelMode();
    return;
  }
  UpdateCancelModeWidgets();
}

// 801A2F98
// The selector beside the cursor's item, its frame tinted red; the last one's frame white again.
void ACTTarget::UpdateCancelModeWidgets() {
  if (!m_cursor && !m_lastCursor) return;
  UIReflow reflow;
  reflow.OpenReflowBlock();
  if (m_cursor) {
    reflow.Reflow_Id("ACT_SelectorIcon", m_player);
    reflow.Reflow_Visibility(true);
    reflow.Reflow_X(m_cursor->m_x + 0x20);
    reflow.Reflow_Y(m_cursor->m_y - 0xe);
    reflow.Reflow_Id(m_cursor->m_frame, m_player);
    reflow.Reflow_ColorOnlyOverride(0xff6666);
  }
  if (m_lastCursor) {
    reflow.Reflow_Id(m_lastCursor->m_frame, m_player);
    reflow.Reflow_ColorOnlyOverride(0xffffff);
  }
  reflow.SubmitReflowBlock();
}

// 801A30A0
void ACTTarget::ExitCancelMode() {
  m_lastCursor = m_cursor;
  m_cursor = nullptr;
  m_cancelMode = 0;
  UpdateCancelModeWidgets();
  UIReflow().ResetVisibility("ACT_SelectorIcon", false, m_player);
  if (_gd.m_h2dTarget) _gd.m_h2dTarget->ACTCancelModeExited(m_player);
}

// 801A311C
// B let go quickly cancelled the last action: its frame turns red (sound 0x048AE94F).
void ACTTarget::CancellingLastObject() {
  if (!m_tail) return;
  UIReflow reflow;
  reflow.OpenReflowBlock();
  reflow.Reflow_Id(m_tail->m_frame, m_player);
  reflow.Reflow_ColorOnlyOverride(0xff6666);
  reflow.SubmitReflowBlock();
  PlaySound(0x48ae94f);
}

// ---------------------------------------------------------------------------------------------------
// INTTarget

// 801F8024
// The target ("<p>INTTarget"), its variables and texts, the meter's shader (0x5625922E) and the
// menu's up arrow (0x90A4EAB9), then the player's screen.
INTTarget::INTTarget(int player) : m_player(player) {
  const std::string pre = std::to_string(player);
  AttachTables(player == 0 ? "0INTTarget" : "1INTTarget");  // 804FE918 INT_TargetName
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_getTable->InstallCommand((pre + "INT_exists").c_str(), [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"INT_show_callback", "INT_hide_callback", "INT_reset_rel", "INT_anim_end"})
    m_setTable->InstallCommand((pre + n).c_str(), set, this);
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"INT_rel", "INT_bonus", "INT_reldata", "INT_sum", "INT_rel2"})
    m_locTable->InstallCommand((pre + n).c_str(), loc, this);
  UI2D::Instance()->InstallEntry(const_cast<char*>((pre + "INT_shader").c_str()), 0x5625922e, const_cast<char*>(m_name), 0);
  UI2D::Instance()->InstallEntry(const_cast<char*>((pre + "INT_CXM_arrow_up").c_str()), 0x90a4eab9,
                                 const_cast<char*>(m_name), 0);
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(m_player == 0 ? 0x3e : 0x3f, 0);
}

// 801F8720
INTTarget::~INTTarget() {
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(m_player == 0 ? 0x3e : 0x3f);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801F88D4
// Faded in (the swf to "a0", animating); faded out (hidden); the relationship back to the bonus
// value; an animation ended.
void INTTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("INT_show_callback", name)) {
    m_tweening = 0;
    UIReflow().ResetSwfAnimation("INT_swf", "a0", m_player);
    m_animating = 1;
  } else if (!std::strcmp("INT_hide_callback", name)) {
    m_tweening = 0;
    UIReflow().ResetVisibility("INT", false, m_player);
    m_shown = 0;
  } else if (!std::strcmp("INT_reset_rel", name)) {
    m_relationship = m_bonus;
    m_relText = Number(m_bonus);
  } else if (!std::strcmp("INT_anim_end", name)) {
    m_animating = 0;
  }
}

// 801F89E4
char* INTTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("INT_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801F8A7C
unsigned short* INTTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("INT_bonus", name)) return Localizable(m_bonusText);
  if (!std::strcmp("INT_reldata", name)) return Localizable(m_relData);
  if (!std::strcmp("INT_sum", name)) return Localizable(m_sumText);
  if (!std::strcmp("INT_rel", name)) return Localizable(m_relText);
  if (!std::strcmp("INT_rel2", name)) return Localizable(m_rel2Text);
  return Localizable(u"");
}

// 801F8BC0
// Fades in once it is wanted and has values (player 1's moved 320 left in two-player mode, the swf at
// "a00", the arrow as the menu has it; the context menu's icon off), out when no longer wanted and no
// animation runs (the icon back on).
void INTTarget::Update() {
  if (!m_exists || m_tweening) return;
  if (m_wantShown) {
    if (!m_shown && m_hasValues) {
      UIReflow r;
      r.OpenTweenBlock("INT", 0xfa, 0, "INT_show_callback", m_player);
      r.Tween_Alpha(0xff);
      r.SubmitTweenBlock();
      m_tweening = 1;
      r.OpenReflowBlock();
      r.Reflow_Id("INT", m_player);
      if (TwoPlayers() && m_player == 0) r.Reflow_X(-0x140);
      r.Reflow_Visibility(true);
      r.Reflow_Id("INT_swf", 0);
      r.Reflow_SwfAnimation(const_cast<char*>("a00"));
      r.Reflow_Id("INT_CXM_arrow_up", m_player);
      r.Reflow_Visibility(m_upArrow != 0);
      r.SubmitReflowBlock();
      if (_gd.m_h2dTarget) _gd.m_h2dTarget->CSIMSetIconDisable(m_player, false);
      m_shown = 1;
      return;
    }
    if (m_wantShown) return;
  }
  if (m_shown && !m_animating) {
    UIReflow r;
    r.OpenTweenBlock("INT", 0xfa, 0, "INT_hide_callback", m_player);
    r.Tween_Alpha(0);
    r.SubmitTweenBlock();
    m_hasValues = 0;
    m_tweening = 1;
    if (_gd.m_h2dTarget) _gd.m_h2dTarget->CSIMSetIconDisable(m_player, true);
  }
}

// 801F8D98
void INTTarget::ShowInterestMeter() { m_wantShown = 1; }
// 801F8DA4
void INTTarget::HideInterestMeter() { m_wantShown = 0; }
// 801F8E8C
int INTTarget::IsInterestMeterActive() { return m_shown; }

// 801F8DB0
// Wanted while the selected sim is in social mode.
void INTTarget::UpdateInterestMeter() {
  if (!m_shown) {
    if (_globals.GetSelectedPerson(m_player) && InSocialMode(m_player)) ShowInterestMeter();
  } else if (!m_tweening) {
    if (_globals.GetSelectedPerson(m_player) && InSocialMode(m_player)) return;
    HideInterestMeter();
  }
}

// 801F8E94
// The interaction's relationship change ("+n" when not 0) and the relationship; a shown meter plays
// "a1".
void INTTarget::SetInteractionDisplayValues(unsigned int change, unsigned int relationship) {
  m_change = static_cast<int>(change);
  m_relationship = static_cast<int>(relationship);
  m_bonusText = change == 0 ? Number(0) : u"+" + Number(static_cast<int>(change));  // DAT_803FA6DC "+"
  m_relText = Number(static_cast<int>(relationship));
  if (m_hasValues && !m_animating) {
    UIReflow().ResetSwfAnimation("INT_swf", "a1", m_player);
    m_animating = 1;
  }
  m_hasValues = 1;
}

// 801F8FDC
// The change, the lifetime value, the bonus and the sum; the meter plays "a2".
void INTTarget::TriggerInteractionAnimation(unsigned int daily, unsigned int lifetime, unsigned int bonus) {
  m_change = static_cast<int>(daily);
  m_lifetime = static_cast<int>(lifetime);
  m_bonus = static_cast<int>(bonus);
  m_sum = static_cast<int>(daily + lifetime);
  m_bonusText = daily == 0 ? Number(0) : u"+" + Number(static_cast<int>(daily));
  m_relData = Number(static_cast<int>(lifetime));
  m_rel2Text = Number(static_cast<int>(bonus));
  m_sumText = Number(static_cast<int>(daily + lifetime));
  UIReflow().ResetSwfAnimation("INT_swf", "a2", m_player);
  m_animating = 1;
}

// 801F9154
void INTTarget::MirrorUpArrow(bool on) {
  if (m_upArrow == static_cast<int>(on)) return;
  m_upArrow = on ? 1 : 0;
  if (m_shown) UIReflow().ResetVisibility("INT_CXM_arrow_up", on, m_player);
}

// ---------------------------------------------------------------------------------------------------
// ActionQueue: the HUD side (game/person.cpp)

// 80119F60
// The visible action at `index` goes to the HUD (with H2D up, HUD access is always on): its icon object's
// shader and where it goes (in front of the next visible one).
void ActionQueue::AddActionToHUD(int index) {
  if (_gd.m_h2dTarget) m_hudAccess = false;  // GC: +0x2B0 cleared (0: access on)
  if (m_hudAccess) return;
  Interaction& action = At(static_cast<unsigned int>(index));
  if (!IsVisibleAction(action)) return;
  ERShader* resource = nullptr;
  cXObject* icon = action.GetIconObject();
  cXObjectImpl* iconImpl = icon ? icon->GetObjectImplementation() : nullptr;
  const unsigned int shader = iconImpl ? GetShaderForObject(iconImpl, &resource) : 0;
  if (!shader && !resource) return;
  const unsigned int before = static_cast<unsigned int>(FindVisualInsertGUID(static_cast<unsigned int>(index)));
  if (H2DTarget* h2d = _gd.m_h2dTarget) {
    const unsigned short* text = action.NameText();
    std::u16string name;
    for (const unsigned short* p = text; p && *p; ++p) name += static_cast<char16_t>(*p);
    h2d->AddAction(m_hudPlayer, &action, name, shader, resource, before);
  }
}

// 8011A06C
void ActionQueue::RemoveActionFromHUD(int index) {
  if (m_hudAccess || static_cast<unsigned int>(index) >= Count()) return;
  if (H2DTarget* h2d = _gd.m_h2dTarget) h2d->RemoveAction(m_hudPlayer, &At(static_cast<unsigned int>(index)));
}

// 8011A2BC
// The unique id of the first visible action from `index` on (0: none).
RET_UNKNOWN ActionQueue::FindVisualInsertGUID(unsigned int index) const {
  for (; index < Count(); ++index)
    if (IsVisibleAction(At(index))) return At(index).m_id;
  return 0;
}

// 8011A770
// The queue reports to `player`'s HUD again: its icons are cleared and every action re-added.
void ActionQueue::EnableHUDAccess(int player) {
  H2DTarget* h2d = _gd.m_h2dTarget;
  if (!h2d || !h2d->IsActionQueueAvailable(player)) return;
  m_hudPlayer = static_cast<unsigned char>(player);
  m_hudAccess = false;
  h2d->RemoveAllActions(player);
  for (unsigned int i = 0; i < Count(); ++i) AddActionToHUD(static_cast<int>(i));
}

namespace {
// ActionQueue::DisableHUDAccess (GC 8011A714; the map lists it static but it works on the queue, so the
// generated declaration is left alone): the HUD's icons for it go, it reports nowhere.
void DisableQueueHUDAccess(ActionQueue& q) {
  if (q.m_hudAccess) return;
  if (H2DTarget* h2d = _gd.m_h2dTarget) h2d->RemoveAllActions(q.m_hudPlayer);
  q.m_hudPlayer = 0xff;
  q.m_hudAccess = true;
}
}  // namespace

// PC stand-in for cXPersonImpl::AttachToHUD (80129804; GC: from SetHilite when the selection flag
// changes, and from H2DTarget's constructor): the player is the one whose selected sim it is (else it
// detaches).
void AttachPersonToHUD(cXPerson* person, bool attach) {
  cXPersonImpl* p = PersonImpl(person);
  if (!p) return;
  int player = 0;
  if (person != _globals.GetSelectedPerson(0)) {
    if (person == _globals.GetSelectedPerson(1)) player = 1;
    else attach = false;
  }
  if (!attach) DisableQueueHUDAccess(p->m_actionQueue);
  else p->m_actionQueue.EnableHUDAccess(player);
}
