// Port of game/uiuserdrawcb.cpp: the table of user draw callbacks for "CCRCB" custom controls.
#include "simmy/types_gen.hpp"

#include <algorithm>
#include <cstring>

UIUserDrawCBTable* UIUserDrawCBTable::s_pInstance = nullptr;
structDrawCBparams* UIUserDrawCBTable::s_drawCBparams = nullptr;  // 80487DA4

namespace {
constexpr size_t kRecords = 0x20;  // the GC pool
}

// 80181DD4
UIUserDrawCBTable::UIUserDrawCBTable() { m_records.reserve(kRecords); }
// 80181E78
UIUserDrawCBTable::~UIUserDrawCBTable() = default;

// 80181F18 (AddRecord)
// 80181F80 (RemoveRecord): the GC record pool; a std::vector here.
// 8018204C
// Fails when the pool is used up or the name is taken.
bool UIUserDrawCBTable::InstallEntry(const char* name, Callback callback, const char* group) {
  if (m_records.size() >= kRecords || FindEntry(name)) return false;
  m_records.push_back(Record{name, std::move(callback), group ? group : ""});
  return true;
}

// 801820C0
bool UIUserDrawCBTable::UnInstallEntry(const char* name) {
  Record* r = FindEntry(name);
  if (!r) return false;
  m_records.erase(m_records.begin() + (r - m_records.data()));
  return true;
}

// 80182104
bool UIUserDrawCBTable::UnInstallAllName(const char* group) {
  const size_t before = m_records.size();
  m_records.erase(std::remove_if(m_records.begin(), m_records.end(),
                                 [&](const Record& r) { return r.group == group; }),
                  m_records.end());
  return m_records.size() != before;
}

// 80182174
UIUserDrawCBTable::Record* UIUserDrawCBTable::FindEntry(const char* name) {
  for (Record& r : m_records)
    if (r.name == name) return &r;
  return nullptr;
}

// 801821D0
// Calls the callback with s_drawCBparams (set up by customControlRender); the hit moves to the front.
void UIUserDrawCBTable::ExecuteCallback(ERC* rc, const char* name) {
  Record* r = FindEntry(name);
  if (!r) return;
  std::rotate(m_records.begin(), m_records.begin() + (r - m_records.data()),
              m_records.begin() + (r - m_records.data()) + 1);
  Callback cb = m_records.front().callback;  // the callback may change the table
  if (cb) cb(rc, s_drawCBparams);
}

// 80182284
UIUserDrawCBTable* UIUserDrawCBTable::Instance() {
  if (!s_pInstance) s_pInstance = new UIUserDrawCBTable;
  return s_pInstance;
}

// 801822C0
void UIUserDrawCBTable::Shutdown() {
  delete s_pInstance;
  s_pInstance = nullptr;
}
