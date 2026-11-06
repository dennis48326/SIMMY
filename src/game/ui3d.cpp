// Port of game/ui3d.cpp: the Flash UI's named 3D displays (see ui3d.hpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

UI3D* UI3D::s_pInstance = nullptr;

// 80176880
// 16 free records and "__3D_UPDATE_" in the update table.
UI3D::UI3D() {
  UI3D__UI3DRecord* last = nullptr;
  for (int i = 0; i < 0x10; ++i) {
    auto* r = new UI3D__UI3DRecord;
    if (!m_free) m_free = r;
    else last->m_next = r, r->m_prev = last;
    last = r;
  }
  UIUpdateTable::InstallEntry(0x4b, [this] { Update3D(); }, "__3D_UPDATE_", this);
}

// 801769E8
// GC: the installed list is joined to the pool and every record deleted (their displays stay).
UI3D::~UI3D() {
  UI3D__UI3DRecord* r = m_free;
  if (m_head) {
    m_tail->m_next = m_free;
    if (m_free) m_free->m_prev = m_tail;
    r = m_head;
  }
  while (r) {
    UI3D__UI3DRecord* next = r->m_next;
    delete r;
    r = next;
  }
  m_head = m_tail = m_free = nullptr;
  if (UIUpdateTable::s_pInstance) UIUpdateTable::s_pInstance->UnInstallEntry(this);
}

// 80176AB4
// A pool record onto the installed list's end, named for the target; a new display (once per record)
// set up for the object at the origin, unturned and still, in the box (0.4, 0.4)-(0.6, 0.6), a light
// (0.95, 0.99, 0.99) in slot 0, ambient 0.25, and the colour.
void UI3D::AddEntry(char* name, unsigned int character, unsigned int model, unsigned int anim, int colorIndex,
                    char* group) {
  UI3D__UI3DRecord* r = m_free;
  if (!r) return;  // GC: the pool of 16 is never exhausted
  m_free = r->m_next;
  if (m_free) m_free->m_prev = nullptr;
  if (!m_head) {
    m_head = r;
    r->m_prev = nullptr;
    m_tail = r;
    r->m_next = nullptr;
  } else {
    m_tail->m_next = r;
    r->m_prev = m_tail;
    m_tail = r;
    r->m_next = nullptr;
  }
  r->m_name = name;
  r->m_colorIndex = colorIndex;
  r->m_group = group;
  r->m_character = character;
  r->m_model = model;
  r->m_anim = anim;
  if (!r->m_display) {
    r->m_display = new CUnlockDisplay;
    ObjectDefinition def;
    def.m_character = character;
    def.m_model = model;
    def.m_anim = anim;
    def.m_field0C = 0;
    EVec3 pos{0.0f, 0.0f, 0.0f}, rot{0.0f, 0.0f, 0.0f};
    EVec3 rotAdd{0.0f, 0.0f, -0.05235988f};  // 0xBD567750: -3 degrees per 1/60 s
    r->m_display->SetupForObject(&def, pos, rot, rotAdd);
    r->m_display->SetDisplayBoxLeft(0.4f);
    r->m_display->SetDisplayBoxTop(0.4f);
    r->m_display->SetDisplayBoxRight(0.6f);
    r->m_display->SetDisplayBoxBottom(0.6f);
    r->m_display->SetDirectLight(0.95f, 0.99f, 0.99f, 0);
    r->m_display->SetAmbientLight(0.25f, 0.25f, 0.25f);
    r->m_display->SetMultiColorNumber(r->m_colorIndex);
  }
}

// 80176C68
// Off the installed list onto the pool's front; its display deleted, the record cleared.
void UI3D::RemoveEntry(UI3D__UI3DRecord* r) {
  if (r->m_prev) r->m_prev->m_next = r->m_next;
  if (r->m_next) r->m_next->m_prev = r->m_prev;
  if (m_head == r) m_head = r->m_next;
  if (m_tail == r) m_tail = r->m_prev;
  r->m_next = nullptr;
  r->m_prev = nullptr;
  if (m_free) {
    r->m_next = m_free;
    m_free->m_prev = r;
  }
  m_free = r;
  if (m_free->m_display) {
    delete m_free->m_display;
    m_free->m_display = nullptr;
  }
  m_free->m_character = 0;
  m_free->m_model = 0;
  m_free->m_anim = 0;
  m_free->m_name = nullptr;
  m_free->m_group = nullptr;
}

// 80176D6C
// The same object under the name again only changes the colour; anything else replaces the entry.
bool UI3D::InstallEntry(char* name, unsigned int character, unsigned int model, unsigned int anim, char* group,
                        int colorIndex) {
  if (!m_free) return false;
  if (UI3D__UI3DRecord* r = FindEntry(name)) {
    if (r->m_anim == anim && r->m_character == character && r->m_model == model) {
      r->m_display->SetMultiColorNumber(colorIndex);
      return true;
    }
    RemoveEntry(r);
  }
  AddEntry(name, character, model, anim, colorIndex, group);
  return true;
}

// 80176E34
// The record by name, moved to the list's front.
UI3D__UI3DRecord* UI3D::FindEntry(char* name) {
  UI3D__UI3DRecord* r = m_head;
  for (; r; r = r->m_next)
    if (r->m_name == name || std::strcmp(r->m_name, name) == 0) break;
  if (!r) return nullptr;
  if (r == m_head) return r;
  if (r == m_tail) m_tail = r->m_prev;
  else r->m_next->m_prev = r->m_prev;
  r->m_prev->m_next = r->m_next;
  r->m_prev = nullptr;
  r->m_next = m_head;
  m_head->m_prev = r;
  m_head = r;
  return r;
}

// 80176EEC
bool UI3D::UnInstallEntry(char* name) {
  UI3D__UI3DRecord* r = FindEntry(name);
  if (r) RemoveEntry(r);
  return r != nullptr;
}

// 80176F30
// Every record of the group (GC compares the group pointers: the target's name).
bool UI3D::UnInstallAllEntries(char* group) {
  bool any = false;
  for (UI3D__UI3DRecord* r = m_head; r;) {
    UI3D__UI3DRecord* next = r->m_next;
    if (r->m_group == group || (r->m_group && group && std::strcmp(r->m_group, group) == 0)) {
      RemoveEntry(r);
      any = true;
    }
    r = next;
  }
  return any;
}

// 80176F90
bool UI3D::ContainsEntry(char* name) { return FindEntry(name) != nullptr; }

// 80176FC0
void UI3D::Update3D() {
  for (UI3D__UI3DRecord* r = m_head; r; r = r->m_next) r->m_display->Update();
  // PC dev aid: SIMMY_UI3D_TRACE=1 prints each display's state every 60 frames.
  static const bool trace = std::getenv("SIMMY_UI3D_TRACE") != nullptr;
  if (trace && _framecount % 60 == 0)
    for (UI3D__UI3DRecord* r = m_head; r; r = r->m_next) {
      CUnlockDisplay* d = r->m_display;
      CUnlockDisplayObject* o = d->m_objects;
      std::printf("[ui3d] %s: model %08X char %08X anim %08X, display state %d radius %.3f, object state %d simple %d "
                  "rot %.2f dt %.4f box %.3f,%.3f-%.3f,%.3f\n",
                  r->m_name, r->m_model, r->m_character, r->m_anim, d->m_state, d->m_radius, o ? o->m_state : -1,
                  o ? o->m_simple : -1, o ? o->m_rot.z : 0.0f, _dt, d->m_boxLeft, d->m_boxTop, d->m_boxRight, d->m_boxBottom);
    }
}

// 80177004
CUnlockDisplay* UI3D::GetDisplayObject(char* name) {
  UI3D__UI3DRecord* r = FindEntry(name);
  return r ? r->m_display : nullptr;
}

// 80177038
UI3D* UI3D::Instance() {
  if (!s_pInstance) s_pInstance = new UI3D;
  return s_pInstance;
}

// 80177074
void UI3D::Shutdown() {
  if (s_pInstance) {
    delete s_pInstance;
    s_pInstance = nullptr;
  }
}
