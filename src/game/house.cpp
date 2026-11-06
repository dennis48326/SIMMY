// Port of game/house.cpp (partial): the house instance (Globs::pHouse) and its family. Loading, saving,
// the lot data and the score curves (HouseImpl::Initialize / LoadFile / SaveFile / Destroy) are the
// world side's stand-ins for now (livemode.cpp BGExec_LoadHouse, lotsave.cpp), so they stay stubs.
#include "simmy/types_gen.hpp"

HouseImpl* Globs_pHouse = nullptr;  // Globs::pHouse

// 800BB358
House* House::CreateInstance() { return new HouseImpl; }  // GC: 0x24 bytes

// 800BB380
void House::DestroyInstance(House* house) { delete house; }

// 800BB3C0
HouseImpl::HouseImpl() = default;

// 800BB430
// GC: Destroy() first (the curves, the room manager, the object module, the simulator and the world).
// PC: those belong to the world side's stand-in (they outlive the house object), so the destructor
// only drops the house's own state.
HouseImpl::~HouseImpl() {
  m_family = nullptr;
  if (Globs_pHouse == this) Globs_pHouse = nullptr;
}

// 800BC984
FamilyImpl* HouseImpl::GetFamily() { return m_family; }
// 800BC98C
PiecewiseFn* HouseImpl::GetSizeScoreCurve() { return m_sizeScoreCurve; }
// 800BC994
PiecewiseFn* HouseImpl::GetFurnishingsScoreCurve() { return m_furnishingsScoreCurve; }
// 800BC99C
void HouseImpl::SetFamilyToNull() { m_family = nullptr; }
