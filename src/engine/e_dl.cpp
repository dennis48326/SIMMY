// Port of engine/e_dl.cpp: display-list storage. The original kept two EAllocGroups (one for data the
// list references, one for the command blocks); on PC both are one list of owned blocks, freed with the
// list. Entries are 16 bytes (see edl.hpp).
#include "simmy/types_gen.hpp"

#include <new>

// 80363014
EDL::EDL(int) {}

// 803630A0
EDL::~EDL() {
  for (const Block& b : m_blocks) ::operator delete(b.ptr, std::align_val_t(b.align));
}

// 8036311C
// Revalidates the textures the list selects (re-uploading any that were evicted) and its children.
void EDL::Validate() {
  for (ETexture* t : m_textures)
    if (t) t->Validate();
  for (EDL* child : m_children)
    if (child) child->Validate();
}

// 80363378
EDLEntry* EDL::GetStart() { return m_start; }

// 80363380
void* EDL::Alloc(unsigned int size, int align) {
  const std::size_t a = align < 16 ? 16 : static_cast<std::size_t>(align);
  void* p = ::operator new(size ? size : 1, std::align_val_t(a));
  m_blocks.push_back({p, a});
  return p;
}

// 803633C0
void* EDL::AllocFlushable(unsigned int size, int align) { return Alloc(size, align); }

// 80363410
double EDL::GetAverageStripLength() { return 0.0; }
