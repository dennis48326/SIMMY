// @class ETypeInfo
// Run-time type information for serialisable classes (base/e_typeinfo.cpp). Each EStorable class
// registers one at static-init time; files name classes by key = EChecksum::Compute(class name).
#pragma once

struct ETypeInfo {
  using NewFn = EStorable* (*)();
  using InPlaceFn = void (*)(void*);

  ETypeInfo* Register(NewFn create, InPlaceFn construct, InPlaceFn destruct, unsigned short version,
                      const char* name, ETypeInfo* parent);
  void Insert();
  static ETypeInfo* Find(unsigned int key);
  int IsDerivedFrom(ETypeInfo* base);

  NewFn m_new = nullptr;              // +0x00
  InPlaceFn m_construct = nullptr;    // +0x04
  InPlaceFn m_destruct = nullptr;     // +0x08
  const char* m_name = nullptr;       // +0x0C
  unsigned int m_key = 0;             // +0x10
  unsigned short m_version = 0;       // +0x14 version this build writes
  unsigned short m_readVersion = 0xffff;  // +0x16 version of the data being read
  ETypeInfo* m_parent = nullptr;      // +0x18
  ETypeInfo* m_left = nullptr;        // +0x1C (binary tree by key)
  ETypeInfo* m_right = nullptr;       // +0x20
  ETypeInfo* m_next = nullptr;        // +0x24 (list of all types)

  static ETypeInfo* m_pTreeHead;
  static ETypeInfo* m_pListHead;
  static int m_count;
};
