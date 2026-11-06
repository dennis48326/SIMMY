// @class FastAllocPool
// A pool of equal-sized blocks carved out of one buffer (base/fastallocpool.cpp). A free block holds
// the tag 'EERF' and the next free block; an allocated one gets the tag 'DESU'.
// PC: the free-list link is a native pointer at +0x08 of the block (GC: a 32-bit word at +0x04), so
// blocks must be at least 16 bytes.
#pragma once

struct FastAllocPool {
  FastAllocPool() = default;
  FastAllocPool(char* name, void* buffer, int blockSize, int blocks);  // 802D6F7C
  virtual ~FastAllocPool();  // 802D6FF0
  void Init(void* buffer, int blockSize, int blocks);  // 802D7024
  int Valid();  // 802D7094
  unsigned int GetAlignment() const;  // 802D7220
  void* GetPoolStart() const;  // 802D7228
  int GetPoolSize() const;  // 802D7230
  int GetBlockSize() const;  // 802D7240
  int GetPoolBlocks() const;  // 802D7248
  bool BlockIsInPool(void* p) const;  // 802D7340
  void* Alloc();  // 802D7374
  void Free(void* p);  // 802D739C

  static constexpr unsigned int kFreeTag = 0x45455246;  // 'EERF'
  static constexpr unsigned int kUsedTag = 0x44455355;  // 'DESU'
  unsigned char* m_start = nullptr;  // +0x00
  void* m_free = nullptr;            // +0x04 the first free block
  int m_blocks = 0;                  // +0x08
  int m_blockSize = 0;               // +0x0C
  unsigned int m_alignment = 0;      // +0x10 the largest power of two dividing the block size and buffer
};
