// @class PropRef
// @class PropTable
// @class PropTableImpl after PropTable
// game/proptable.cpp: an object file's prop tables (its ResFile +0x08: {table, id} pairs sorted by id;
// the tree primitive 0x06 "change suit" reads table 0x130, the props a tree can dress a sim in: the
// vacuum, a book, a drink, the extinguisher...). A prop is a model drawn with the sim's bones
// (SAnimator2::Dress / AddProp, DrawProps).
#pragma once

// One prop as the object file holds it (0x10 bytes, big-endian, read in place): +0 the adult's model,
// +4 the child's (0: the adult's), +0xC a byte: also drawn in the shadow (and the "flagged" props pass).
struct PropRef {
  unsigned int GetAdultModel() const { return ERQuickdata::U32(this, 0); }
  unsigned int GetChildModel() const { return ERQuickdata::U32(this, 4); }
  bool GetCastsShadow() const { return ERQuickdata::U8(this, 0xc) != 0; }
};

// The interface (GC vtable: +0x14 Load, +0x24 GetEntry).
struct PropTable {
  virtual ~PropTable() = default;
  static PropTable* CreateInstance();  // 80132810
  static void DestroyInstance(PropTable* table);  // 80132838
  // The table `id` of the namespace (PC: its ResFile row; GC: a namespace id the object folder resolves,
  // vtable 0x3C GetFileByID). 0, or -1 without one.
  virtual int Load(const unsigned char* resFile, short id) = 0;
  virtual const PropRef* GetEntry(int index) = 0;
  virtual short GetID() = 0;
  virtual int CountEntries() const = 0;
  virtual const char* GetEntryName(int index) = 0;
};

struct PropTableImpl : PropTable {
  PropTableImpl();  // 80132878
  ~PropTableImpl() override;  // 80132894
  int Load(const unsigned char* resFile, short id) override;  // 801328C8
  const PropRef* GetEntry(int index) override;  // 80132950
  short GetID() override;  // 80132984
  int CountEntries() const override;  // 80132990
  const char* GetEntryName(int index) override;  // 801329AC

  const unsigned char* m_table = nullptr;  // +0x04 the {props, id} pair found (PropRefTable)
};
