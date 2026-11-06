// Port of base/e_typeinfo.cpp.
#include "simmy/types_gen.hpp"

#include <cstring>

ETypeInfo* ETypeInfo::m_pTreeHead = nullptr;  // 804FF040
ETypeInfo* ETypeInfo::m_pListHead = nullptr;  // 804FF044
int ETypeInfo::m_count = 0;                   // 804FF048

// 802D58CC
ETypeInfo* ETypeInfo::Register(NewFn create, InPlaceFn construct, InPlaceFn destruct, unsigned short version,
                               const char* name, ETypeInfo* parent) {
  m_new = create;
  m_construct = construct;
  m_destruct = destruct;
  m_name = name;
  m_key = static_cast<unsigned int>(EChecksum::Compute(const_cast<char*>(name)));
  m_version = version;
  m_readVersion = 0xffff;
  m_parent = parent == this ? nullptr : parent;
  ++m_count;
  Insert();
  return this;
}

// 802D5954
void ETypeInfo::Insert() {
  m_next = m_pListHead;
  m_pListHead = this;
  ETypeInfo* node = m_pTreeHead;
  ETypeInfo* at = nullptr;
  while (node) {
    at = node;
    if (node->m_key == m_key) return;
    node = m_key < node->m_key ? node->m_left : node->m_right;
  }
  if (!at) m_pTreeHead = this;
  else if (m_key < at->m_key) at->m_left = this;
  else at->m_right = this;
  m_left = m_right = nullptr;
}

// 802D59DC
ETypeInfo* ETypeInfo::Find(unsigned int key) {
  ETypeInfo* node = m_pTreeHead;
  while (node && node->m_key != key) node = key < node->m_key ? node->m_left : node->m_right;
  return node;
}

// 802D5A10
int ETypeInfo::IsDerivedFrom(ETypeInfo* base) {
  for (ETypeInfo* t = this; t; t = t->m_parent)
    if (t == base) return 1;
  return 0;
}
