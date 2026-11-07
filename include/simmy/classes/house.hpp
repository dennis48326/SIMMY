// @class House
// @class HouseImpl after House
// The house being played (game/house.cpp; Globs::pHouse): its family, its name and the lot score curves.
// Ported: the instance, the family, its messages (DoCommand), IsNative / IsResident, RefreshHouse,
// EnterLiveMode, the lot data; loading and saving are the world side's stand-ins (livemode.cpp
// BGExec_LoadHouse, lotsave.cpp).
#pragma once

struct House {
  House() = default;
  virtual ~House() = default;
  static House* CreateInstance();  // 800BB358
  static void DestroyInstance(House* house);  // 800BB380
};

struct HouseImpl : House {
  HouseImpl();  // 800BB3C0
  ~HouseImpl() override;  // 800BB430
  virtual void Initialize();  // 800BB4AC
  virtual void Destroy();  // 800BB6A8
  virtual void RefreshHouse();  // 800BB760
  virtual void SetLotSize(int, int);  // 800BB7D4
  RET_UNKNOWN LoadFile(iResFile *, int *);  // 800BB8CC
  virtual void ComputeAndStoreLotData();  // 800BBAE8
  RET_UNKNOWN SaveFile(iResFile *);  // 800BBC3C
  virtual void GetFirstObject();  // 800BBD08
  virtual RET_UNKNOWN DoCommand(short, int);  // 800BBD40
  virtual bool IsNative(cXPerson *);  // 800BBF9C
  virtual RET_UNKNOWN IsResident(cXPerson *);  // 800BC00C
  virtual void EnterLiveMode();  // 800BC0E0
  virtual void PrepareForBudgetWindow();  // 800BC154
  virtual void DoStream(ReconBuffer *, int);  // 800BC1B8
  virtual void GetHouseStats(HouseStats &);  // 800BC24C
  virtual FamilyImpl* GetFamily();  // 800BC984
  virtual PiecewiseFn* GetSizeScoreCurve();  // 800BC98C
  virtual PiecewiseFn* GetFurnishingsScoreCurve();  // 800BC994
  virtual void SetFamilyToNull();  // 800BC99C

  // +0x04 Commander, +0x10 its vtable (not ported)
  FamilyImpl* m_family = nullptr;              // +0x14 the family living here (NeighborhoodImpl::LoadHouse)
  // +0x18 BString the house's name (PC: lotsave.cpp keeps it with the 'HOUS' resource)
  PiecewiseFn* m_sizeScoreCurve = nullptr;     // +0x1C
  PiecewiseFn* m_furnishingsScoreCurve = nullptr;  // +0x20
};

extern HouseImpl* Globs_pHouse;  // Globs::pHouse (GC: a House*)

// PC: GlobalDispatch (800B42BC) to the house, the one Commander the port has for these messages
// (HouseImpl::DoCommand: 0xEC, 0xED, 0xEF, 0xD8, 0xD9, 0xDE, 0xFE, 0x100).
void HouseDispatch(short message, int param);
