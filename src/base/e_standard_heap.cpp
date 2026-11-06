// Port of base/e_standard_heap.cpp: EA's named heaps, rewritten for PC on the host allocator.
//
// The GameCube build carved fixed address ranges out of the 24 MB arena (see
// ENgcEngine::InitMemoryManager). On PC every heap forwards to aligned host allocations. A small
// header in front of each block records the owning heap and size, so Free() and
// FindHeapForAddress() work without the original allocator. Budgets are kept as information only.
// The game's malloc/free/new/delete wrappers in this file are not ported; the host CRT provides them.
#include "simmy/types_gen.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
struct BlockHeader {
  EAHeap* heap;
  std::size_t size;
  void* raw;        // what the host allocator returned
  std::size_t pad;  // keeps the header 32 bytes so 16-byte alignment is preserved
};
static_assert(sizeof(BlockHeader) == 32);
constexpr std::uint32_t kAllocMagicMask = 0;  // reserved for debug checks

int g_checkPoint = 0;

void* host_alloc(EAHeap* heap, std::size_t size, std::size_t align) {
  if (align < 16) align = 16;
  const std::size_t total = size + sizeof(BlockHeader) + align;
  auto* raw = static_cast<std::uint8_t*>(std::malloc(total));
  if (!raw) return nullptr;
  auto user = reinterpret_cast<std::uintptr_t>(raw) + sizeof(BlockHeader);
  user = (user + align - 1) & ~(static_cast<std::uintptr_t>(align) - 1);
  auto* hdr = reinterpret_cast<BlockHeader*>(user) - 1;
  hdr->heap = heap;
  hdr->size = size;
  hdr->raw = raw;
  hdr->pad = kAllocMagicMask;
  const std::size_t used = heap->m_used.fetch_add(size) + size;
  heap->m_count.fetch_add(1);
  std::size_t peak = heap->m_peak.load();
  while (used > peak && !heap->m_peak.compare_exchange_weak(peak, used)) {
  }
  return reinterpret_cast<void*>(user);
}

BlockHeader* header_of(void* p) { return static_cast<BlockHeader*>(p) - 1; }
}  // namespace

// ---- EAHeap ------------------------------------------------------------------------------------

// 802D09DC
EAHeap::EAHeap(char* name, void* base, unsigned int size, EAHeap* parent, EAHeap* unused)
    : m_name(name ? name : ""), m_parent(parent), m_budget(size) {}

// 802D0A5C
int EAHeap::Init(void* base, unsigned int size) {
  m_budget = size;
  return 1;
}

// 802D0AA4
void* EAHeap::Malloc(unsigned int size, int flags) { return host_alloc(this, size, 16); }

// 802D0B70
void* EAHeap::MallocAligned(unsigned int size, unsigned int align, unsigned int offset, int flags) {
  // `offset` asks for (ptr + offset) to be aligned; only 0 is used by the game code seen so far.
  return host_alloc(this, size, align);
}

// 802D0C84
void* EAHeap::Calloc(unsigned int count, unsigned int size, int flags) {
  void* p = host_alloc(this, static_cast<std::size_t>(count) * size, 16);
  if (p) std::memset(p, 0, static_cast<std::size_t>(count) * size);
  return p;
}

// 802D0D78
void EAHeap::Free(void* p) {
  if (!p) return;
  BlockHeader* hdr = header_of(p);
  EAHeap* owner = hdr->heap;
  owner->m_used.fetch_sub(hdr->size);
  owner->m_count.fetch_sub(1);
  std::free(hdr->raw);
}

// 802D0E28
void EAHeap::Compact() {}  // nothing to compact with a host allocator

// 802D0E88
void EAHeap::Clear() {
  // The GameCube version reset the whole arena. Nothing in the boot path relies on it.
  std::fprintf(stderr, "[heap] Clear() requested on %s (ignored on PC)\n", m_name);
}

// 802D0ED4
int EAHeap::ValidateHeap(EA__Allocator__GeneralAllocator__HeapValidationLevel) { return 1; }

// 802D0F24
EAHeap* EAHeap::FindHeapForAddress(void* p) const { return p ? header_of(p)->heap : nullptr; }

// 802D0FA0
void EAHeap::ValidatePools() {}

// 802D09AC
void EAHeap::SetNextCheckPoint() { ++g_checkPoint; }

// 802D09D4
int EAHeap::GetCurrentCheckPoint() { return g_checkPoint; }

// 802D070C
void EAHeap::InitializeHeapStatics() {}

// ---- FPHeap ------------------------------------------------------------------------------------

// 802D0FC4
FPHeap::FPHeap(char* name, void* base, unsigned int size, EAHeap* parent, EAHeap* unused)
    : EAHeap(name, base, size, parent, unused) {}

// 802D0FF4
void* FPHeap::Malloc(unsigned int size, int flags) { return EAHeap::Malloc(size, flags); }

// 802D1058
void FPHeap::Free(void* p) { EAHeap::Free(p); }

// 802D10C4
void FPHeap::ValidatePools() {}

// 802D08F8
void FPHeap::InitializeHeapStatics() {}

// ---- named heaps (function-local statics, as in the original) -----------------------------------

#define SIMMY_HEAP(fn, name, parentFn)                                   \
  EAHeap* fn() {                                                         \
    static EAHeap heap(const_cast<char*>(name), nullptr, 0, parentFn, nullptr); \
    return &heap;                                                        \
  }

// 802CFE64
EAHeap* RootHeap() {
  static EAHeap heap(const_cast<char*>("RootHeap"), nullptr, 0, nullptr, nullptr);
  return &heap;
}
// 802CFF00
SIMMY_HEAP(MainHeap, "MainHeap", RootHeap())
// 802CFFA0
SIMMY_HEAP(ResourceHeap, "ResourceHeap", RootHeap())
// 802D0040
SIMMY_HEAP(VMHeap, "VMHeap", RootHeap())
// 802D00E0
SIMMY_HEAP(DebugHeap, "DebugHeap", RootHeap())
// 802D017C
SIMMY_HEAP(CUIHeap, "CUIHeap", RootHeap())
// 802D021C
SIMMY_HEAP(AudioHeap, "AudioHeap", RootHeap())
// 802D02CC
SIMMY_HEAP(ShaderHeap, "ShaderHeap", RootHeap())
// 802D037C
SIMMY_HEAP(TextureHeap, "TextureHeap", RootHeap())
// 802D042C
SIMMY_HEAP(AnimationHeap, "AnimationHeap", RootHeap())
// 802D04DC
SIMMY_HEAP(ModelHeap, "ModelHeap", RootHeap())
// 802D058C
SIMMY_HEAP(FlashesHeap, "FlashesHeap", RootHeap())
// 802D062C
SIMMY_HEAP(AptHeap, "AptHeap", RootHeap())
