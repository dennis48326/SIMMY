// Port of game/proptable.cpp: PropTable / PropTableImpl (an object file's prop tables). See
// include/simmy/classes/proptable.hpp.
#include "simmy/types_gen.hpp"

namespace {
// 803ABB7C FindRes<PropRefTable>: a binary search of the {props, id} pairs (8 bytes each, sorted by the
// short at +4) for `id`; null when there is none.
const unsigned char* FindRes(const unsigned char* begin, const unsigned char* end, int id) {
  while (true) {
    const long n = (end - begin) >> 3;
    if (n < 1) return nullptr;
    if (n == 1) break;
    const unsigned char* mid = begin + (n / 2) * 8;
    const int key = ERQuickdata::S16(mid, 4);
    if (id == key) return mid;
    if (id - key > 0) begin = mid + 8;
    else end = mid;
  }
  return ERQuickdata::S16(begin, 4) == id ? begin : nullptr;
}
}  // namespace

// 80132810
PropTable* PropTable::CreateInstance() { return new PropTableImpl; }

// 80132838
void PropTable::DestroyInstance(PropTable* table) { delete table; }

// 80132878
PropTableImpl::PropTableImpl() {}

// 80132894
PropTableImpl::~PropTableImpl() {}

// 801328C8
// The namespace's prop tables (its ResFile +0x08: count at -4), table `id` of them.
int PropTableImpl::Load(const unsigned char* resFile, short id) {
  const auto* tables = resFile ? static_cast<const unsigned char*>(ERQuickdata::Ptr(resFile, 8)) : nullptr;
  const unsigned int count = tables ? ERQuickdata::U32(tables - 4, 0) : 0;
  const unsigned char* found = tables ? FindRes(tables, tables + count * 8, id) : nullptr;
  if (!found) return -1;
  m_table = found;
  return 0;
}

// 80132950
// Prop `index` of the table (its props: pointers, count at -4); null past the end.
const PropRef* PropTableImpl::GetEntry(int index) {
  const auto* props = m_table ? static_cast<const unsigned char*>(ERQuickdata::Ptr(m_table, 0)) : nullptr;
  const int count = props ? static_cast<int>(ERQuickdata::U32(props - 4, 0)) : 0;
  if (index < 0 || count <= index) return nullptr;
  return static_cast<const PropRef*>(ERQuickdata::Ptr(props, static_cast<unsigned int>(index) * 4));
}

// 80132984
short PropTableImpl::GetID() { return ERQuickdata::S16(m_table, 4); }

// 80132990
int PropTableImpl::CountEntries() const {
  const auto* props = m_table ? static_cast<const unsigned char*>(ERQuickdata::Ptr(m_table, 0)) : nullptr;
  return props ? static_cast<int>(ERQuickdata::U32(props - 4, 0)) : 0;
}

// 801329AC
const char* PropTableImpl::GetEntryName(int) { return nullptr; }
