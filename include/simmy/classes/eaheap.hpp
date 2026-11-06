// @class EAHeap
// PC replacement for EA's heap (base/e_standard_heap.cpp). Allocations go to the host allocator;
// each heap keeps its name, parent and usage counters so the game's diagnostics still work.
#pragma once
#include <atomic>
#include <cstddef>

struct EAHeap {
  EAHeap() = default;
  EAHeap(char* name, void* base, unsigned int size, EAHeap* parent, EAHeap* unused);
  virtual ~EAHeap() = default;

  static void InitializeHeapStatics();
  static void SetNextCheckPoint();
  static int GetCurrentCheckPoint();
  int Init(void* base, unsigned int size);
  void* Malloc(unsigned int size, int flags);
  void* MallocAligned(unsigned int size, unsigned int align, unsigned int offset, int flags);
  void* Calloc(unsigned int count, unsigned int size, int flags);
  void Free(void* p);
  void Compact();
  void Clear();
  int ValidateHeap(EA__Allocator__GeneralAllocator__HeapValidationLevel level);
  EAHeap* FindHeapForAddress(void* p) const;
  static void ValidatePools();

  const char* m_name = "";
  EAHeap* m_parent = nullptr;
  unsigned int m_budget = 0;            // size the GameCube build gave this heap (informational)
  std::atomic<std::size_t> m_used{0};   // live bytes
  std::atomic<std::size_t> m_peak{0};
  std::atomic<std::size_t> m_count{0};  // live allocations
};
