// Port of game/careers.cpp: the career table, its strings and the behaviour data. See careers.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <string>

Careers* Globs_pCareers = nullptr;

namespace {
const unsigned int kCareerQuickdata = 0x4f40c4ecu;
bool Empty(const unsigned short* const* slot) { return !slot || !*slot || !**slot; }
const unsigned short kEmpty[1] = {0};
// Entry `i` of a WStringSet row's list (+0: the string slots, count at [-4]), else null.
const unsigned short* const* SetString(const void* row, int i) {
  const auto* list = row ? static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 0)) : nullptr;
  const int n = list ? static_cast<int>(ERQuickdata::U32(list - 4, 0)) : 0;
  return i >= 0 && i < n ? ERQuickdata::LocSlot(list, static_cast<unsigned int>(i) * 4) : nullptr;
}
std::u16string Text(const unsigned short* const* slot) {
  std::u16string s;
  for (const unsigned short* t = slot ? *slot : nullptr; t && *t; ++t) s += static_cast<char16_t>(*t);
  return s;
}
}  // namespace

// 800B1B54
Careers::Careers() {}
// 800B13EC
Careers::~Careers() {}

// 800B1420
Careers* Careers::CreateInstance() { return new CareersImpl; }

// 800B1460
void Careers::DestroyInstance(Careers* careers) { delete careers; }

// 800B1B68
CareersImpl::CareersImpl() {}
// 800B1B7C
CareersImpl::~CareersImpl() { TearDown(); }

// 800B1500
// The quickdata, its "Career" table and the "Grades" and "Performance" string sets (GC: also an empty
// list at +0x14).
void CareersImpl::Load() {
  TearDown();
  m_qd = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(kCareerQuickdata, nullptr, 0));
  if (!m_qd) return;
  m_careers = m_qd->getTable("Career");
  const void* sets = m_qd->getTable("WStringSet");
  m_grades = sets ? m_qd->getRow(sets, const_cast<char*>("Grades")) : nullptr;
  m_performance = sets ? m_qd->getRow(sets, const_cast<char*>("Performance")) : nullptr;
}

// 800B15F8
void CareersImpl::TearDown() {
  if (!m_qd) return;
  _quickdataman.DelRef(m_qd, EResourceManager__DelRefMode{0});
  m_performance = nullptr;
  m_qd = nullptr;
  m_careers = nullptr;
  m_grades = nullptr;
}

// 800B1644
const Career* CareersImpl::GetCareerByID(int id) {
  const int n = GetNumCareers();
  for (int i = 0; i < n; ++i) {
    const Career* c = GetCareerByIndex(i);
    if (career::Id(c) == id) return c;
  }
  return nullptr;
}

// 800B1698
int CareersImpl::GetNumCareers() { return m_careers ? static_cast<int>(ERQuickdata::TableRowCount(m_careers)) : 0; }

// 800B16A4
const Career* CareersImpl::GetCareerByIndex(int index) {
  return static_cast<const Career*>(ERQuickdata::TableRow(m_careers, static_cast<unsigned int>(index)));
}

// 800B16B8
int CareersImpl::GetIndexByCareer(const Career* c) {
  return static_cast<int>((reinterpret_cast<const unsigned char*>(c) - ERQuickdata::TableRows(m_careers)) / 0x14);
}

// 800B16D8
const unsigned short* const* CareersImpl::GetJobPerformance(int performance) {
  const float p = static_cast<float>(performance);
  int band = 4;
  if (p < -60.0f) band = 0;
  else if (p < -30.0f) band = 1;
  else if (p < 30.0f) band = 2;
  else if (p < 60.0f) band = 3;
  m_emptyText = kEmpty;
  const unsigned short* const* s = SetString(m_performance, band);
  return s ? s : &m_emptyText;
}

// 800B179C
const unsigned short* const* CareersImpl::GetJobGrade(int grade) {
  m_emptyText = kEmpty;
  const unsigned short* const* s = SetString(m_grades, grade);
  return s ? s : &m_emptyText;
}

// 800B17D0
void CareersImpl::GetOfferDialogText(unsigned short* out, const Career& career, int level, bool female) {
  const Career* c = &career;
  std::u16string s = Text(ERQuickdata::LocSlot(c, 0xc));
  const Job* job = career::JobAt(c, level);
  s += Text(ERQuickdata::LocSlot(job, female ? 0x5c : 0x58));
  for (char16_t ch : s) *out++ = static_cast<unsigned short>(ch);
  *out = 0;
}

// 800B1AB0
const unsigned short* const* CareersImpl::GetJobName(const Job* job, bool female) {
  if (female && !Empty(ERQuickdata::LocSlot(job, 0x5c))) return ERQuickdata::LocSlot(job, 0x5c);
  return ERQuickdata::LocSlot(job, 0x58);
}

// 800B1AE8
const unsigned short* const* CareersImpl::GetShortName(const Job* job, bool female) {
  if (female && !Empty(ERQuickdata::LocSlot(job, 0x64))) return ERQuickdata::LocSlot(job, 0x64);
  return ERQuickdata::LocSlot(job, 0x60);
}

// 800B1B20
// An hour before work.
int CareersImpl::GetCarpoolHour(const Job* job) { return (career::StartHour(job) + 0x17) % 0x18; }

// 800B1B4C
const void* CareersImpl::GetSuit(const Job* job, bool) { return ERQuickdata::Ptr(job, 0x68); }

// 800B1944
int CareersImpl::GetBehCareerData(const Career* c, int level, int data, short* out) {
  if (data == 0) {
    *out = static_cast<short>(career::JobCount(c));
    return 1;
  }
  if (level < 0 || level >= career::JobCount(c)) return 0;
  const Job* job = career::JobAt(c, level);
  // The low half of each (big-endian) int field.
  if (data < 1) return 0;
  if (data == 1) *out = ERQuickdata::S16(job, 0x46);
  else if (data < 0xc) *out = ERQuickdata::S16(job, static_cast<unsigned int>((data - 2) * 4 + 2));
  else if (data == 0xc) *out = ERQuickdata::S16(job, 0x4a);
  else if (data == 0xd) *out = ERQuickdata::S16(job, 0x4e);
  else if (data < 0x15) *out = ERQuickdata::S16(job, static_cast<unsigned int>((data - 0xe) * 4 + 0x2a));
  else if (data == 0x15) *out = ERQuickdata::S16(job, 0x52);
  else if (data == 0x16) {
    const char* suit = static_cast<const char*>(GetSuit(job, false));
    *out = suit && suit[0] ? 1 : 0;
  } else if (data == 0x17) *out = ERQuickdata::S16(job, 0x56);
  else return 0;
  return 1;
}
