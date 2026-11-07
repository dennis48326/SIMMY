// GENERATED stubs for unported functions of engine/e_rmodel.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void CalcShadowBounds(ESubModelShader &, EBound3 &) { SIMMY_STUB("CalcShadowBounds(ESubModelShader &, EBound3 &)", 0x80317B08); }
void CalcShadowBounds(TArray<ESubModel, TArrayERModelAllocator> &, EBound3 &) { SIMMY_STUB("CalcShadowBounds(TArray<ESubModel, TArrayERModelAllocator> &, EBound3 &)", 0x80317CC4); }
RET_UNKNOWN operator>>(EStream &, ERModel *&) { SIMMY_STUB("operator>>(EStream &, ERModel *&)", 0x80317D6C); return {}; }
RET_UNKNOWN operator>>(EFile &, ERModel *&) { SIMMY_STUB("operator>>(EFile &, ERModel *&)", 0x80317DAC); return {}; }
void ERModel::GetWeldPos(EWeldVert *) { SIMMY_STUB("ERModel::GetWeldPos(EWeldVert *)", 0x80319214); }
void ERModel::SetWeldPos(EWeldVert *, EVec3 &) { SIMMY_STUB("ERModel::SetWeldPos(EWeldVert *, EVec3 &)", 0x803192C0); }
void ERModel::WeldSharedVerts(ERModel *, float) { SIMMY_STUB("ERModel::WeldSharedVerts(ERModel *, float)", 0x80319350); }
void ERModel::RebuildAfterWelding() { SIMMY_STUB("ERModel::RebuildAfterWelding(void)", 0x80319E84); }
RET_UNKNOWN ERModel::GetAttachmentID(unsigned char, signed char &) { SIMMY_STUB("ERModel::GetAttachmentID(unsigned char, signed char &)", 0x8031A7FC); return {}; }
RET_UNKNOWN ERModel::DrawAttachment(ERC *, ERModel *, EMat4 *, int, float) { SIMMY_STUB("ERModel::DrawAttachment(ERC *, ERModel *, EMat4 *, int, float)", 0x8031A84C); return {}; }
void ERModel::GetAttachmentMatrix(EMat4 &, ERModel *, EMat4 *, int, float) { SIMMY_STUB("ERModel::GetAttachmentMatrix(EMat4 &, ERModel *, EMat4 *, int, float)", 0x8031A8C0); }
RET_UNKNOWN ERModel::GetAttachmentMatrix(EMat4 &, unsigned char, EMat4 *, int, float, float) { SIMMY_STUB("ERModel::GetAttachmentMatrix(EMat4 &, unsigned char, EMat4 *, int, float, float)", 0x8031A964); return {}; }
void ERModel::RegisterType(unsigned short) { SIMMY_STUB("ERModel::RegisterType(unsigned short)", 0x8031B2EC); }
void ERModel::CreateCopy() const { SIMMY_STUB("ERModel::CreateCopy(void) const", 0x8031B340); }
void ERModel::CalcOrientedBoundBox(EMat4 &, EBound3 &) { SIMMY_STUB("ERModel::CalcOrientedBoundBox(EMat4 &, EBound3 &)", 0x8031B3C4); }
RET_UNKNOWN ERModel::GetLattice(int) { SIMMY_STUB("ERModel::GetLattice(int)", 0x8031B3F8); return {}; }
RET_UNKNOWN ERModel::GetDummy(int) { SIMMY_STUB("ERModel::GetDummy(int)", 0x8031B408); return {}; }
RET_UNKNOWN ERModel::GetCamera(int) { SIMMY_STUB("ERModel::GetCamera(int)", 0x8031B418); return {}; }
