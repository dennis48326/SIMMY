// GENERATED stubs for unported functions of game/camera.cpp. Do not edit.
#include "simmy/types_gen.hpp"

bool anon::IsUsingSpecifiedInteractor(int, InteractorModule__InteractorType) { SIMMY_STUB("{anonymous}::IsUsingSpecifiedInteractor(int, InteractorModule::InteractorType)", 0x800159A0); return {}; }
void anon::IsUsingDirectControlInteractor(int) { SIMMY_STUB("{anonymous}::IsUsingDirectControlInteractor(int)", 0x80015A08); }
void anon::IsUsingSocialModeInteractor(int) { SIMMY_STUB("{anonymous}::IsUsingSocialModeInteractor(int)", 0x80015A2C); }
void anon::HandleEffectNote(ESimsCam *, EAnimNote &, anim__PropKind) { SIMMY_STUB("{anonymous}::HandleEffectNote(ESimsCam *, EAnimNote &, anim::PropKind)", 0x80015A50); }
void SimsCameraParameters::Validate() { SIMMY_STUB("SimsCameraParameters::Validate(void)", 0x80016044); }
CameraMotionSystem::CameraMotionSystem(SimsCameraParameters *) { SIMMY_STUB("CameraMotionSystem::CameraMotionSystem(SimsCameraParameters *)", 0x8001606C); }
CameraMotionSystem::~CameraMotionSystem() { SIMMY_STUB("CameraMotionSystem::~CameraMotionSystem(void)", 0x80016080); }
void CameraMotionSystem::SetMotionType(CameraMotionType) { SIMMY_STUB("CameraMotionSystem::SetMotionType(CameraMotionType)", 0x800160A8); }
RET_UNKNOWN CameraMotionSystem::GetMotionType() { SIMMY_STUB("CameraMotionSystem::GetMotionType(void)", 0x800160B0); return {}; }
void CameraMotionSystem::SetRig(EVec3 &, EVec3 &, EVec3 &) { SIMMY_STUB("CameraMotionSystem::SetRig(EVec3 &, EVec3 &, EVec3 &)", 0x800160B8); }
RET_UNKNOWN CameraMotionSystem::SnapCamera() { SIMMY_STUB("CameraMotionSystem::SnapCamera(void)", 0x80016110); return {}; }
void CameraMotionSystem::MoveCamera(float) { SIMMY_STUB("CameraMotionSystem::MoveCamera(float)", 0x8001621C); }
void CameraMotionSystem::RubberBandInterpolate(EVec3 *, EVec3, float) { SIMMY_STUB("CameraMotionSystem::RubberBandInterpolate(EVec3 *, EVec3, float)", 0x80016258); }
void CameraMotionSystem::RubberBandMove(float) { SIMMY_STUB("CameraMotionSystem::RubberBandMove(float)", 0x8001639C); }
void ESimsCam::SetState(Panelstateman__Panelstate) { SIMMY_STUB("ESimsCam::SetState(Panelstateman::Panelstate)", 0x80016B54); }
void ESimsCam::SetPosAndReset(EVec3 &, EVec3 &, EVec3 &) { SIMMY_STUB("ESimsCam::SetPosAndReset(EVec3 &, EVec3 &, EVec3 &)", 0x80018E74); }
void ESimsCam::ForcePosition(EVec3 &, EVec3 &, EVec3 &) { SIMMY_STUB("ESimsCam::ForcePosition(EVec3 &, EVec3 &, EVec3 &)", 0x8001916C); }
void ESimsCam::ForceTarget(EVec3 &) { SIMMY_STUB("ESimsCam::ForceTarget(EVec3 &)", 0x800192FC); }
void ESimsCam::PointOffScreen(EVec3 &) { SIMMY_STUB("ESimsCam::PointOffScreen(EVec3 &)", 0x80019364); }
void ESimsCam::PointOutsideScreenRect(EVec3 &, float, float, float, float) { SIMMY_STUB("ESimsCam::PointOutsideScreenRect(EVec3 &, float, float, float, float)", 0x80019398); }
RET_UNKNOWN ESimsCam::ValidateCursorPosition(int, EVec3 *) { SIMMY_STUB("ESimsCam::ValidateCursorPosition(int, EVec3 *)", 0x8001942C); return {}; }
RET_UNKNOWN ESimsCam::GetContainingRoomId(EVec3 &) { SIMMY_STUB("ESimsCam::GetContainingRoomId(EVec3 &)", 0x8001976C); return {}; }
RET_UNKNOWN ESimsCam::CursorMoved(int, EVec3 &) { SIMMY_STUB("ESimsCam::CursorMoved(int, EVec3 &)", 0x80019A00); return {}; }
RET_UNKNOWN ESimsCam::GetCursorPos() { SIMMY_STUB("ESimsCam::GetCursorPos(void)", 0x80019F6C); return {}; }
double ESimsCam::GetCursorLimitTopWidth() { SIMMY_STUB("ESimsCam::GetCursorLimitTopWidth(void)", 0x8001A03C); return {}; }
double ESimsCam::GetCursorLimitDepth() { SIMMY_STUB("ESimsCam::GetCursorLimitDepth(void)", 0x8001A074); return {}; }
void ESimsCam::GetObjectPosition(EVec3 *, cXObject *) { SIMMY_STUB("ESimsCam::GetObjectPosition(EVec3 *, cXObject *)", 0x8001A0AC); }
void ESimsCam::GetObjectOrientation(EVec3 *, cXObject *) { SIMMY_STUB("ESimsCam::GetObjectOrientation(EVec3 *, cXObject *)", 0x8001A168); }
void ESimsCam::SetAnimationMode(CameraAnimationMode, cXObject *, cXObject *, bool) { SIMMY_STUB("ESimsCam::SetAnimationMode(CameraAnimationMode, cXObject *, cXObject *, bool)", 0x8001A22C); }
RET_UNKNOWN ESimsCam::DrawDebug(ERC *) { SIMMY_STUB("ESimsCam::DrawDebug(ERC *)", 0x8001A24C); return {}; }
RET_UNKNOWN ESimsCam::CursorNotActive() { SIMMY_STUB("ESimsCam::CursorNotActive(void)", 0x8001A2B8); return {}; }
void ESimsCam::DrawCameraInfo(ERC *) { SIMMY_STUB("ESimsCam::DrawCameraInfo(ERC *)", 0x8001AA74); }
RET_UNKNOWN ESimsCam::SetControllerFilterId(unsigned int) { SIMMY_STUB("ESimsCam::SetControllerFilterId(unsigned int)", 0x8001AD94); return {}; }
RET_UNKNOWN ESimsCam::GetControllerFilterId() const { SIMMY_STUB("ESimsCam::GetControllerFilterId(void) const", 0x8001ADA0); return {}; }
RET_UNKNOWN ESimsCam::ClearControllerFilterId() { SIMMY_STUB("ESimsCam::ClearControllerFilterId(void)", 0x8001ADA8); return {}; }
void CameraDirector::InterpToCancelCamera(float, CamDirInterpType, bool) { SIMMY_STUB("CameraDirector::InterpToCancelCamera(float, CamDirInterpType, bool)", 0x8001C840); }
RET_UNKNOWN CameraDirector::CalcSimsCam() { SIMMY_STUB("CameraDirector::CalcSimsCam(void)", 0x8001C8D4); return {}; }
void CameraDirector::Draw(ERC *) { SIMMY_STUB("CameraDirector::Draw(ERC *)", 0x8001DFE0); }
RET_UNKNOWN CameraDirector::ProcessAnimEvents(AnimRef *, int, int) { SIMMY_STUB("CameraDirector::ProcessAnimEvents(AnimRef *, int, int)", 0x8001E2E0); return {}; }
RET_UNKNOWN CameraDirector::AnimEventHandler(AnimRef *, EAnimNote &, int) { SIMMY_STUB("CameraDirector::AnimEventHandler(AnimRef *, EAnimNote &, int)", 0x8001E3E0); return {}; }
void CameraDirector::BeginCameraBloomInterp(CameraBloomDataElement *) { SIMMY_STUB("CameraDirector::BeginCameraBloomInterp(CameraBloomDataElement *)", 0x8001E4C0); }
void CameraDirector::BloomInterp() { SIMMY_STUB("CameraDirector::BloomInterp(void)", 0x8001E540); }
void CameraDirector::BlurInterp() { SIMMY_STUB("CameraDirector::BlurInterp(void)", 0x8001E5F0); }
NewControlParms::NewControlParms() { SIMMY_STUB("NewControlParms::NewControlParms(void)", 0x8001E6DC); }
void NewControlParms::Validate() { SIMMY_STUB("NewControlParms::Validate(void)", 0x8001E878); }
ESimsCam::ESimsCam() { SIMMY_STUB("ESimsCam::ESimsCam(void)", 0x8001E944); }
RET_UNKNOWN ESimsCam::GetEye() const { SIMMY_STUB("ESimsCam::GetEye(void) const", 0x8001EA2C); return {}; }
RET_UNKNOWN ESimsCam::GetTarget() const { SIMMY_STUB("ESimsCam::GetTarget(void) const", 0x8001EA34); return {}; }
RET_UNKNOWN ESimsCam::GetUp() const { SIMMY_STUB("ESimsCam::GetUp(void) const", 0x8001EA3C); return {}; }
RET_UNKNOWN ESimsCam::GetForward() const { SIMMY_STUB("ESimsCam::GetForward(void) const", 0x8001EA44); return {}; }
void ESimsCam::SetTarget(EVec3 &) { SIMMY_STUB("ESimsCam::SetTarget(EVec3 &)", 0x8001EA4C); }
double ESimsCam::GetZoom() const { SIMMY_STUB("ESimsCam::GetZoom(void) const", 0x8001EA74); return {}; }
double ESimsCam::GetTilt() const { SIMMY_STUB("ESimsCam::GetTilt(void) const", 0x8001EA7C); return {}; }
void ESimsCam::SetTilt(float) { SIMMY_STUB("ESimsCam::SetTilt(float)", 0x8001EA84); }
RET_UNKNOWN ESimsCam::GetbMoved() const { SIMMY_STUB("ESimsCam::GetbMoved(void) const", 0x8001EA8C); return {}; }
double ESimsCam::GetRotAng() const { SIMMY_STUB("ESimsCam::GetRotAng(void) const", 0x8001EA94); return {}; }
RET_UNKNOWN ESimsCam::GetMode() const { SIMMY_STUB("ESimsCam::GetMode(void) const", 0x8001EA9C); return {}; }
void ESimsCam::ForceFullScreen(bool) { SIMMY_STUB("ESimsCam::ForceFullScreen(bool)", 0x8001EAAC); }
RET_UNKNOWN ESimsCam::IsForceFullScreen() { SIMMY_STUB("ESimsCam::IsForceFullScreen(void)", 0x8001EAB4); return {}; }
void ESimsCam::SetCenterNew(bool) { SIMMY_STUB("ESimsCam::SetCenterNew(bool)", 0x8001EABC); }
RET_UNKNOWN ESimsCam::GetRoomId() { SIMMY_STUB("ESimsCam::GetRoomId(void)", 0x8001EAC4); return {}; }
RET_UNKNOWN ESimsCam::GetRoomWallsOpaque() { SIMMY_STUB("ESimsCam::GetRoomWallsOpaque(void)", 0x8001EACC); return {}; }
void ESimsCam::SetInterpolatingToOrFromSocialModeCameraFlag(bool) { SIMMY_STUB("ESimsCam::SetInterpolatingToOrFromSocialModeCameraFlag(bool)", 0x8001EAD4); }
void ESimsCam::SetInterpolatingFromLotIntroCameraFlag(bool) { SIMMY_STUB("ESimsCam::SetInterpolatingFromLotIntroCameraFlag(bool)", 0x8001EADC); }
