// Port of base/fastallocpool.cpp: FastAllocPool (ProtectedAllocPool, the mutex-guarded variant, is
// not used by the ported code and stays stubbed).
#include "simmy/types_gen.hpp"

#include <cstdint>
#include <cstring>

namespace {
// PC: the free-list link (GC: the block's second word).
void*& Link(void* block) { return *reinterpret_cast<void**>(static_cast<unsigned char*>(block) + 8); }
void Tag(void* block, unsigned int tag) { std::memcpy(block, &tag, 4); }
}  // namespace

// 802D6F7C
FastAllocPool::FastAllocPool(char*, void* buffer, int blockSize, int blocks) {
  if (buffer && blockSize && blocks) Init(buffer, blockSize, blocks);
}

// 802D6FF0
FastAllocPool::~FastAllocPool() {}

// 802D7024
// Every block of the buffer onto the free list (the last one first out).
void FastAllocPool::Init(void* buffer, int blockSize, int blocks) {
  const unsigned int a = static_cast<unsigned int>(blockSize) |
                         (static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(buffer)) & 0xffffff);
  m_free = nullptr;
  m_alignment = a & (0u - a);
  m_start = static_cast<unsigned char*>(buffer);
  m_blocks = blocks;
  m_blockSize = blockSize;
  unsigned char* p = m_start;
  for (int i = 0; i < m_blocks; ++i, p += blockSize) {
    if (!p) continue;
    Tag(p, kFreeTag);
    Link(p) = m_free;
    m_free = p;
  }
}

// 802D7094
// Every block on the free list lies in the buffer (GC: a debugger break otherwise).
int FastAllocPool::Valid() {
  for (void* p = m_free; p; p = Link(p))
    if (!BlockIsInPool(p)) return 0;
  return 1;
}

// 802D7220
unsigned int FastAllocPool::GetAlignment() const { return m_alignment; }
// 802D7228
void* FastAllocPool::GetPoolStart() const { return m_start; }
// 802D7230
int FastAllocPool::GetPoolSize() const { return m_blockSize * m_blocks; }
// 802D7240
int FastAllocPool::GetBlockSize() const { return m_blockSize; }
// 802D7248
int FastAllocPool::GetPoolBlocks() const { return m_blocks; }

// 802D7340
bool FastAllocPool::BlockIsInPool(void* p) const {
  const auto* b = static_cast<const unsigned char*>(p);
  return m_start <= b && b < m_start + m_blockSize * m_blocks;
}

// 802D7374
void* FastAllocPool::Alloc() {
  void* p = m_free;
  if (!p) return nullptr;
  m_free = Link(p);
  Tag(p, kUsedTag);
  return p;
}

// 802D739C
void FastAllocPool::Free(void* p) {
  if (!p) return;
  Tag(p, kFreeTag);
  Link(p) = m_free;
  m_free = p;
}
