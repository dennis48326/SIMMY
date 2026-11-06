// @class FPHeap
// Fixed-pool heap; on PC it is an ordinary EAHeap.
#pragma once

struct FPHeap : public EAHeap {
  FPHeap() = default;
  FPHeap(char* name, void* base, unsigned int size, EAHeap* parent, EAHeap* unused);
  static void InitializeHeapStatics();
  void* Malloc(unsigned int size, int flags);
  void Free(void* p);
  static void ValidatePools();
};
