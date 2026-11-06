// Port of game/cascostumes.cpp (CasCostumes): the outfit descriptions an outfit change dresses a sim in.
// Not ported: saving the list and making a new one (the CAS NPC editor's), UpdateSimDescription.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstring>

namespace {
constexpr unsigned int kCostumesBinary = 0x4c967a29u;
constexpr int kCostumesResType = 0x43435354;  // 'CCST'

void NameOf(const CasSimDescriptionS2C* d, char* out) {
  CopyWStringToCharStr(const_cast<unsigned short*>(d->m_firstName), out, 0x20);
}

struct CostumesObject : ReconObject {
  CasCostumes* m_costumes;
  explicit CostumesObject(CasCostumes* c) : m_costumes(c) {}
  void DoStream(ReconBuffer* b, int version) override { m_costumes->DoStream(b, version); }
  int GetType() override { return kCostumesResType; }
};
}  // namespace

// 8015F524
CasCostumes::CasCostumes() {}

// 8015F55C
CasCostumes::~CasCostumes() { ClearCostumesList(); }

// 8015F5E0
bool CasCostumes::CompareSimDescription(CasSimDescriptionS2C* a, CasSimDescriptionS2C* b) {
  if (!a || !b) return true;
  char na[0x20], nb[0x20];
  NameOf(a, na);
  NameOf(b, nb);
  return std::strcmp(na, nb) < 0;
}

// 8015F654
void CasCostumes::ClearCostumesList() {
  for (auto& list : m_lists) {
    for (CasSimDescriptionS2C*& d : list) {
      delete d;
      d = nullptr;
    }
    list.clear();
  }
}

// 8015F718
int CasCostumes::LoadCostumesList() {
  EResource* res = _binaryman.EResourceManager::AddRef(kCostumesBinary, nullptr, 0);
  if (!res) {
    CreateNewCostumesList();
    return 1;
  }
  auto* bin = static_cast<ERBinary*>(res);
  Memory__HandleNode h;
  h.m_data = static_cast<unsigned char*>(bin->GetData());
  h.m_size = bin->GetDataSize();
  h.m_owned = 0;
  CostumesObject object(this);
  int version = 2;
  ReconBuilder().Reconstitute(&object, &h, &version);
  res->DelRef();
  return 1;
}

// 8015F948
// A version word, the two lists' counts, then each description (DescriptionS2C::DoStream). Read: the
// lists made anew and sorted by name (version 1 lists also get two default costumes per gender, named
// from the CostumeName table; not ported, the disc's list is version 2).
void CasCostumes::DoStream(ReconBuffer* b, int version) {
  int listVersion = 2;
  b->Recon32(&listVersion, 1);
  short counts[2];
  for (int i = 0; i < 2; ++i) {
    counts[i] = static_cast<short>(m_lists[i].size());
    b->Recon16(&counts[i], 1);
  }
  if (b->m_mode != ReconBuffer__Mode::kRead) {
    for (auto& list : m_lists)
      for (CasSimDescriptionS2C* d : list) d->DoStream(b, version);
    return;
  }
  ClearCostumesList();
  for (int i = 0; i < 2; ++i) {
    m_lists[i].reserve(0x51);
    for (short n = 0; n < counts[i]; ++n) {
      auto* d = new CasSimDescriptionS2C;
      m_lists[i].push_back(d);
      d->DoStream(b, version);
    }
    if (listVersion == 1) SIMMY_STUB("CasCostumes::DoStream: version 1 lists' default costumes", 0x8015F948);
    std::sort(m_lists[i].begin(), m_lists[i].end(), CompareSimDescription);
  }
}

// 80160040
CasSimDescriptionS2C* CasCostumes::GetSimDescription(bool male, const char* name) {
  CasSimDescriptionS2C key;
  CopyCharStrToWString(const_cast<char*>(name), key.m_firstName, 0x20);
  auto& list = m_lists[male ? 0 : 1];
  auto range = std::equal_range(list.begin(), list.end(), &key, CompareSimDescription);
  if (range.first == list.end()) return std::strncmp(name, "PJ", 2) == 0 ? nullptr : GetSimDescription(male, "PJ");
  return *range.first;
}

// 8016010C
CasSimDescriptionS2C* CasCostumes::GetSimDescription(bool male, unsigned int index) {
  auto& list = m_lists[male ? 0 : 1];
  return index < list.size() ? list[index] : nullptr;
}
