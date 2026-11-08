// Port of game/house.cpp (partial): the house instance (Globs::pHouse), its family, its messages
// (DoCommand), residents, entering live mode and the lot data. Loading, saving and the score curves
// (HouseImpl::Initialize / LoadFile / SaveFile / Destroy) are the world side's stand-ins
// (livemode.cpp BGExec_LoadHouse, lotsave.cpp), so they stay stubs.
//
// GC: HouseImpl is a Commander: GlobalDispatch hands it every broadcast. PC: GlobalDispatch has no
// listener list; the places that broadcast the messages it handles call HouseDispatch.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

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

// 800BC0E0
// The router starts afresh: the rooms' partitions go, their statistics are recomputed, every route is
// planned again and the portals forget their scores; then the lot data is stored.
void HouseImpl::EnterLiveMode() {
  if (Globs_pRoomManager) {
    Globs_pRoomManager->ClearRoomPartitions();
    Globs_pRoomManager->AllRoomsScoreChanged();
    Globs_pRoomManager->ComputeRooms();
  }
  if (Globs_pObjectModule) Globs_pObjectModule->InvalidateAllRoutes();
  ComputeAndStoreLotData();
}

// 800BB760
// The rooms' scores and the world's rooms again, the 3D house rebuilt, then the lot data.
void HouseImpl::RefreshHouse() {
  if (Globs_pRoomManager) Globs_pRoomManager->AllRoomsScoreChanged();
  if (Globs_pFixedWorld) Globs_pFixedWorld->ComputeRooms(0);  // vtable 0x114
  if (Globs_pRoomManager) Globs_pRoomManager->ComputeRooms();
  _globals.RecalcHouse();
  ComputeAndStoreLotData();  // vtable 0x8C
}

// 800BBD40
// A broadcast: the family hears it first (FamilyImpl::MyDoCommand, vtable 0x2C); then 0xEC the world's
// rooms changed on a level (the objects' rooms, ObjectModule vtable 0x7C, and the rooms' statistics),
// 0xD8 a day passed (the lot data), 0xD9 the time of day changed (every room's lighting), 0xDE an object
// was made (a person: the people's visitor types, Neighborhood vtable 0x9C), 0xFE a family's house
// changed (if it is this house, simulator global 9: the house's family again), 0xED a room's lighting,
// 0xEF a room's score (not the 0xFFFB "no room"), 0x100 the lot's size (high and low halves),
// 0x86 nothing, 0x3F4 the rooms' statistics printed. 1: handled.
RET_UNKNOWN HouseImpl::DoCommand(short message, int param) {
  if (m_family) m_family->MyDoCommand(message, param);
  switch (message) {
    case 0xec:
      if (Globs_pObjectModule) Globs_pObjectModule->UpdateRooms(param);
      if (Globs_pRoomManager) Globs_pRoomManager->ComputeRooms();
      return 1;
    case 0xd8: ComputeAndStoreLotData(); return 1;
    case 0x86: return 1;
    case 0xd9:
      if (Globs_pRoomManager) Globs_pRoomManager->AllRoomsLightingChanged();
      return 1;
    case 0xde: {
      cXObject* o = Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(param) : nullptr;
      if (o && o->GetObjectImplementation() && o->GetObjectImplementation()->GetType() == 2 && Globs_pNeighborhood)
        Globs_pNeighborhood->UpdateInstanceVisitorTypes();
      return 1;
    }
    case 0xfe:
      if (Globs_pSimulator && param == Globs_pSimulator->GetGlobal(9)) {
        Globs_pSimulator->SetGlobal(9, 0);
        m_family = Globs_pNeighborhood ? Globs_pNeighborhood->GetFamily(0) : nullptr;
      }
      return 1;
    case 0xed:
      if (Globs_pRoomManager) Globs_pRoomManager->RoomLightingChanged(param);
      return 1;
    case 0xef:
      if (param != 0xfffb && Globs_pRoomManager) Globs_pRoomManager->RoomScoreChanged(param);
      return 1;
    case 0x100: SetLotSize(param >> 16, param & 0xffff); return 1;
    case 0x3f4:
      SIMMY_STUB("HouseImpl::DoCommand: RoomManager::PrintStats (a debug message)", 0x800BBD40);
      return 1;
    default: return 0;
  }
}

// 800BBF9C
// A member of the house's family.
bool HouseImpl::IsNative(cXPerson* person) { return m_family && m_family->TestMember(person); }

// 800BC00C
// A member of the house's family, or (outside free play, DAT_80475C6C) of family 1, the player's.
RET_UNKNOWN HouseImpl::IsResident(cXPerson* person) {
  // PC: a player's selected sim lives here. On GC it is always a family member (player 2's sim is the
  // roommate PreGameState::InsertPlayer2Roommate moved in); the port's fallback for a game started
  // without one (MoveFamilyOntoLot: a new neighbour in no family) would otherwise be a visitor whose
  // trees walk it off the lot.
  for (int player = 0; person && player < 2; ++player)
    if (_globals.GetSelectedPerson(player) == person) return 1;
  // GC: Globs::pHouse (vtable 0x3C GetFamily), not this.
  FamilyImpl* family = Globs_pHouse ? Globs_pHouse->GetFamily() : nullptr;
  if (family && family->TestMember(person)) return 1;
  if (_gd.m_fieldE8 == 0 && Globs_pNeighborhood) {
    FamilyImpl* player = Globs_pNeighborhood->GetFamily(1);
    if (player && player->TestMember(person)) return 1;
  }
  return 0;
}

// 800BC154
// The lot data again, unless the simulator is in mode 1.
void HouseImpl::PrepareForBudgetWindow() {
  if (Globs_pSimulator && Globs_pSimulator->GetMode() == 1) return;
  ComputeAndStoreLotData();  // vtable 0x8C
}

// PC: GlobalDispatch's listener list (Commander::sList) for the house: the messages HouseImpl::DoCommand
// handles, where the port broadcasts them.
void HouseDispatch(short message, int param) {
  if (Globs_pHouse) Globs_pHouse->DoCommand(message, param);
}

// ObjectModuleImpl::DoCommand's 0xEE: the object (by id) in a room (not 0xFFFB) has its room's lighting
// recomputed (GlobalDispatch 0xED: HouseImpl::DoCommand -> RoomManager::RoomLightingChanged).
void ObjectModuleDispatch(short message, int param) {
  if (message != 0xee || !Globs_pObjectModule) return;
  cXObject* x = Globs_pObjectModule->GetObjectFromID(param);
  cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
  if (!o) return;
  const int room = static_cast<int>(o->GetRoom());
  if (room == 0xfffb) return;
  HouseDispatch(0xed, room);
}

