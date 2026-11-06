// GENERATED stubs for unported functions of game/e_rlevel.cpp. Do not edit.
#include "simmy/types_gen.hpp"

EOrderTableEntry::EOrderTableEntry() { SIMMY_STUB("EOrderTableEntry::EOrderTableEntry(void)", 0x8023122C); }
RET_UNKNOWN operator>>(EStream &, ERLevel *&) { SIMMY_STUB("operator>>(EStream &, ERLevel *&)", 0x802312C8); return {}; }
RET_UNKNOWN operator>>(EFile &, ERLevel *&) { SIMMY_STUB("operator>>(EFile &, ERLevel *&)", 0x80231308); return {}; }
void ERLevel::Write(EStream &) { SIMMY_STUB("ERLevel::Write(EStream &)", 0x802318F0); }
void ERLevel::AddInstancesFromList(TNodeList<EInstance *> &) { SIMMY_STUB("ERLevel::AddInstancesFromList(TNodeList<EInstance *> &)", 0x80231B90); }
void ERLevel::AddInstancesFromBoundTree(EStorable *) { SIMMY_STUB("ERLevel::AddInstancesFromBoundTree(EStorable *)", 0x80231BD0); }
void ERLevel::Read(EStream &) { SIMMY_STUB("ERLevel::Read(EStream &)", 0x80231C7C); }
void ERLevel::Refresh(EFile *) { SIMMY_STUB("ERLevel::Refresh(EFile *)", 0x80232BB0); }
void ERLevel::ClearJustReadByLevelFlags() { SIMMY_STUB("ERLevel::ClearJustReadByLevelFlags(void)", 0x80232DF8); }
void ERLevel::DestroyInstancesWithoutJustReadByLevelFlags() { SIMMY_STUB("ERLevel::DestroyInstancesWithoutJustReadByLevelFlags(void)", 0x80232E3C); }
void ERLevel::DestroyInstancesOriginallyReadByLevel() { SIMMY_STUB("ERLevel::DestroyInstancesOriginallyReadByLevel(void)", 0x80232F84); }
void ERLevel::AddInstanceToLevelHead(EInstance *) { SIMMY_STUB("ERLevel::AddInstanceToLevelHead(EInstance *)", 0x8023353C); }
void ERLevel::FixInstances() { SIMMY_STUB("ERLevel::FixInstances(void)", 0x80233A38); }
void ERLevel::FixInstanceList(TNodeList<EInstance *> &) { SIMMY_STUB("ERLevel::FixInstanceList(TNodeList<EInstance *> &)", 0x80233B44); }
void ERLevel::FindInstance(char *) { SIMMY_STUB("ERLevel::FindInstance(char *)", 0x80233C50); }
RET_UNKNOWN ERLevel::FindInstance(unsigned int) { SIMMY_STUB("ERLevel::FindInstance(unsigned int)", 0x80233C90); return {}; }
void ERLevel::FindFirstInstance(unsigned int, EInstance **) { SIMMY_STUB("ERLevel::FindFirstInstance(unsigned int, EInstance **)", 0x80233CCC); }
void ERLevel::FindNextInstance(HTIteratorPtrType *, EInstance **) { SIMMY_STUB("ERLevel::FindNextInstance(HTIteratorPtrType *, EInstance **)", 0x80233CF4); }
void ERLevel::AddBounds(EBound3 &, EBound3 &, bool &) { SIMMY_STUB("ERLevel::AddBounds(EBound3 &, EBound3 &, bool &)", 0x802341B0); }
RET_UNKNOWN ERLevel::CalcBounds() { SIMMY_STUB("ERLevel::CalcBounds(void)", 0x80234260); return {}; }
void ERLevel::CalcRadiusFromViewParams(float, float, float) { SIMMY_STUB("ERLevel::CalcRadiusFromViewParams(float, float, float)", 0x80234438); }
RET_UNKNOWN OrderTableSortReferenceCompare(void *, void *) { SIMMY_STUB("OrderTableSortReferenceCompare(void *, void *)", 0x802344B0); return {}; }
void ERLevel::DrawOrderTableSlotFast(EOrderTableData *) { SIMMY_STUB("ERLevel::DrawOrderTableSlotFast(EOrderTableData *)", 0x80234AE4); }
void ERLevel::DrawSortedOrderTable(EOrderTableData *) { SIMMY_STUB("ERLevel::DrawSortedOrderTable(EOrderTableData *)", 0x80234BA4); }
RET_UNKNOWN FindOTD(EOrderTableData *, EOrderTableData &) { SIMMY_STUB("FindOTD(EOrderTableData *, EOrderTableData &)", 0x80236320); return {}; }
RET_UNKNOWN ERLevel::IntersectBoundBox(EBound3 &, EVec3 &, EVec3 &) { SIMMY_STUB("ERLevel::IntersectBoundBox(EBound3 &, EVec3 &, EVec3 &)", 0x80236930); return {}; }
void ERLevel::DrawWireFrame(ERC *) { SIMMY_STUB("ERLevel::DrawWireFrame(ERC *)", 0x80236BEC); }
void ERLevel::RegisterType(unsigned short) { SIMMY_STUB("ERLevel::RegisterType(unsigned short)", 0x8023813C); }
void ERLevel::CreateCopy() const { SIMMY_STUB("ERLevel::CreateCopy(void) const", 0x80238190); }
void ERLevel::SetWallFadeState(EWallUpDownStateType) { SIMMY_STUB("ERLevel::SetWallFadeState(EWallUpDownStateType)", 0x80238250); }
RET_UNKNOWN ERLevel::WallFadeState() { SIMMY_STUB("ERLevel::WallFadeState(void)", 0x8023825C); return {}; }
void ERLevel::SetIdMapTableSize(int) { SIMMY_STUB("ERLevel::SetIdMapTableSize(int)", 0x80238268); }
RET_UNKNOWN ERLevel::IsInUse() { SIMMY_STUB("ERLevel::IsInUse(void)", 0x80238298); return {}; }
void ERLevel::UpdateRooms(int) { SIMMY_STUB("ERLevel::UpdateRooms(int)", 0x802382B0); }
void ERLevel::SetRainBlend(float) { SIMMY_STUB("ERLevel::SetRainBlend(float)", 0x802382E0); }
RET_UNKNOWN ERLevel::GetStorableVersion() const { SIMMY_STUB("ERLevel::GetStorableVersion(void) const", 0x802382EC); return {}; }
RET_UNKNOWN ERLevel::GetNumVisibleInstances() { SIMMY_STUB("ERLevel::GetNumVisibleInstances(void)", 0x80238368); return {}; }
RET_UNKNOWN ERLevel::GetVisInstance(int) { SIMMY_STUB("ERLevel::GetVisInstance(int)", 0x80238374); return {}; }
