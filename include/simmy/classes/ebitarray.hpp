// @class EBitArray
// A growable array of bits (base/e_bitarray.cpp), 0x14 bytes on the GameCube: the words, the size in
// bits, the capacity, the grow step and the vtable (+0x10). Bit i is bit (i & 31) of word i >> 5, so a
// field of n bits starting at bit p is read least significant bit first. Animations keep their
// compressed key streams in one (ERAnim +0x50); the decompressors read fields out of it.
#pragma once
#include <vector>

struct EBitArray {
  EBitArray();  // 802DF544
  EBitArray(int);  // 802DF594
  EBitArray(int, int);  // 802DF5F8
  EBitArray(EBitArray &);  // 802DF65C
  virtual void AllocateMemory(unsigned long);  // 802DF6BC
  virtual void FreeMemory(void *);  // 802DF6F4
  void Deallocate();  // 802DF72C
  bool Get(int bit) const;  // 802DF780
  void Set(int, bool);  // 802DF7C0
  void SetAll(bool);  // 802DF7FC
  void InvertAll();  // 802DF884
  void SetGrowBy(int growBy);  // 802DF8DC
  void SetSize(int, int);  // 802DF8EC
  void Insert(bool, int);  // 802DFA30
  void Set(EBitArray &, int, int, int);  // 802DFA7C
  void Insert(EBitArray &, int, int, int);  // 802DFAE8
  RET_UNKNOWN InsertElements(int, int);  // 802DFB44
  void Remove(int, int);  // 802DFBF0
  RET_UNKNOWN operator=(EBitArray &);  // 802DFC6C
  RET_UNKNOWN GetElementCount() const;  // 802DFCC4
  void operator|=(EBitArray &);  // 802DFCD4
  void operator&=(EBitArray &);  // 802DFD3C
  void operator^=(EBitArray &);  // 802DFDA4
  RET_UNKNOWN operator==(EBitArray &) const;  // 802DFE0C
  RET_UNKNOWN Intersection(EBitArray &) const;  // 802DFE98
  void Interleave(int, int, int);  // 802DFF0C
  void Deinterleave(int, int, int);  // 802DFFDC
  static void Print();  // 802E00AC
  void Add(unsigned int, int);  // 802E00B0
  void Add(float);  // 802E011C
  unsigned int Get(int bit, int count) const;  // 802E0148
  float GetFloat(int bit) const;  // 802E01F0
  int GetSigned(int bit, int count) const;  // 802E0220
  static void ToleranceToSignedBits(float);  // 802E02A0
  static void ToleranceToUnsignedBits(float);  // 802E032C
  static void MaxNumberToUnsignedBits(int);  // 802E03C0
  RET_UNKNOWN FloatToSignedBits(float, int);  // 802E03F4
  RET_UNKNOWN FloatToUnsignedBits(float, int);  // 802E048C
  static float SignedBitsToFloatScaler(int bits);  // 802E050C
  static float UnsignedBitsToFloatScaler(int bits);  // 802E0558
  RET_UNKNOWN GetAllocByteCount();  // 802E05A0
  virtual ~EBitArray();  // 802E06F8
  void FreeUnusedBufferSpace();  // 802E0744
  int GetSize() const;  // 802E076C
  void Insert(EBitArray &, int);  // 802E0774
  void Add(bool);  // 802E07A0
  void Add(EBitArray &);  // 802E07C4
  void Add(EBitArray &, int, int);  // 802E07F0
  bool IsEmpty() const;  // 802E0818
  void Empty();  // 802E0828
  void RemoveAll();  // 802E0850
  void Clear(int);  // 802E0878
  void ClearAll();  // 802E089C
  void operator[](int) const;  // 802E08C0
  void operator[](int);  // 802E08E0
  RET_UNKNOWN operator!=(EBitArray &) const;  // 802E08F0
  RET_UNKNOWN GetBuffer();  // 802E0914

  // PC: the words are held in a vector (GC: AllocateMemory/FreeMemory on the main heap).
  std::vector<unsigned int> m_words;  // +0x00 host-endian words
  int m_size = 0;                     // +0x04 bits in use
  int m_growBy = 0x1000;              // +0x0C
};
