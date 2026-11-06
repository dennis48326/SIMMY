// GENERATED stubs for unported functions of game/eyetoyclient.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void EyeToyClient::saveHeader(NghResFile__SaveRecord &) { SIMMY_STUB("EyeToyClient::saveHeader(NghResFile::SaveRecord &)", 0x80037400); }
void EyeToyClient::saveTexture(int, NghResFile__SaveRecord &) { SIMMY_STUB("EyeToyClient::saveTexture(int, NghResFile::SaveRecord &)", 0x800374AC); }
void EyeToyClient::Save(NghResFile__SectionID, NghResFile__SaveRecord &) { SIMMY_STUB("EyeToyClient::Save(NghResFile::SectionID, NghResFile::SaveRecord &)", 0x800375A0); }
void EyeToyClient::SaveComplete(NghResFile__SectionID, NghResFile__SaveRecord &) { SIMMY_STUB("EyeToyClient::SaveComplete(NghResFile::SectionID, NghResFile::SaveRecord &)", 0x80037650); }
void EyeToyClient::loadHeader(NghResFile__SaveRecord &) { SIMMY_STUB("EyeToyClient::loadHeader(NghResFile::SaveRecord &)", 0x80037654); }
RET_UNKNOWN EyeToyClient::loadTexture_alloc(unsigned int, unsigned int) { SIMMY_STUB("EyeToyClient::loadTexture_alloc(unsigned int, unsigned int)", 0x80037708); return {}; }
void EyeToyClient::loadTexture_dealloc(void *) { SIMMY_STUB("EyeToyClient::loadTexture_dealloc(void *)", 0x80037710); }
void EyeToyClient::loadTexture(int, NghResFile__SaveRecord &) { SIMMY_STUB("EyeToyClient::loadTexture(int, NghResFile::SaveRecord &)", 0x80037714); }
void EyeToyClient::Load(NghResFile__SectionID, NghResFile__SaveRecord &) { SIMMY_STUB("EyeToyClient::Load(NghResFile::SectionID, NghResFile::SaveRecord &)", 0x80037834); }
void EyeToyClient::LoadDefaultSettings() { SIMMY_STUB("EyeToyClient::LoadDefaultSettings(void)", 0x800378E4); }
void EyeToyClient::FrameReadyCallback(char *, int) { SIMMY_STUB("EyeToyClient::FrameReadyCallback(char *, int)", 0x80037930); }
RET_UNKNOWN EyeToyClient::FindFirstConnectedEyeToy() { SIMMY_STUB("EyeToyClient::FindFirstConnectedEyeToy(void)", 0x80037970); return {}; }
void EyeToyClient::AcquireTexture(int, int, int) { SIMMY_STUB("EyeToyClient::AcquireTexture(int, int, int)", 0x800379D0); }
void EyeToyClient::ReleaseTexture(ETexture *) { SIMMY_STUB("EyeToyClient::ReleaseTexture(ETexture *)", 0x80037A5C); }
void EyeToyClient::AcquireRenderSurface(int, int, int) { SIMMY_STUB("EyeToyClient::AcquireRenderSurface(int, int, int)", 0x80037A9C); }
void EyeToyClient::ReleaseRenderSurface(ERenderSurface *) { SIMMY_STUB("EyeToyClient::ReleaseRenderSurface(ERenderSurface *)", 0x80037B40); }
void EyeToyClient::CopyTextureToShader(ERShader *, ETexture *) { SIMMY_STUB("EyeToyClient::CopyTextureToShader(ERShader *, ETexture *)", 0x80037B80); }
void EyeToyClient::CompositeFF_callback(ETexture *) { SIMMY_STUB("EyeToyClient::CompositeFF_callback(ETexture *)", 0x80037D20); }
void EyeToyClient::UpdateFunFrameLoading() { SIMMY_STUB("EyeToyClient::UpdateFunFrameLoading(void)", 0x80038120); }
void EyeToyClient::SetFilterPipeline(int) { SIMMY_STUB("EyeToyClient::SetFilterPipeline(int)", 0x8003819C); }
void EyeToyClient::SetUnplugCallback(void (*)(int)) { SIMMY_STUB("EyeToyClient::SetUnplugCallback(void (*)(int))", 0x800384E0); }
void EyeToyClient::UnpluggedCallback(int) { SIMMY_STUB("EyeToyClient::UnpluggedCallback(int)", 0x800384E8); }
void EyeToyClient::Init() { SIMMY_STUB("EyeToyClient::Init(void)", 0x80038518); }
void EyeToyClient::Shutdown() { SIMMY_STUB("EyeToyClient::Shutdown(void)", 0x80038634); }
void EyeToyClient::Update(float) { SIMMY_STUB("EyeToyClient::Update(float)", 0x80038710); }
RET_UNKNOWN EyeToyClient::MacroBlockDummyAllocator(unsigned int, unsigned int) { SIMMY_STUB("EyeToyClient::MacroBlockDummyAllocator(unsigned int, unsigned int)", 0x800387B4); return {}; }
void EyeToyClient::MacroBlockDummyFree(void *) { SIMMY_STUB("EyeToyClient::MacroBlockDummyFree(void *)", 0x800387BC); }
void EyeToyClient::Render() { SIMMY_STUB("EyeToyClient::Render(void)", 0x800387C0); }
RET_UNKNOWN EyeToyClient::GetFunframeCount() { SIMMY_STUB("EyeToyClient::GetFunframeCount(void)", 0x80038CD8); return {}; }
RET_UNKNOWN EyeToyClient::GetFilterCount() { SIMMY_STUB("EyeToyClient::GetFilterCount(void)", 0x80038CE0); return {}; }
RET_UNKNOWN EyeToyClient::GetTargetSlotCount() { SIMMY_STUB("EyeToyClient::GetTargetSlotCount(void)", 0x80038CE8); return {}; }
RET_UNKNOWN EyeToyClient::OpenSession() { SIMMY_STUB("EyeToyClient::OpenSession(void)", 0x80038CF0); return {}; }
void EyeToyClient::CloseSession() { SIMMY_STUB("EyeToyClient::CloseSession(void)", 0x80038DC4); }
void EyeToyClient::ApplyPresetFilter(int) { SIMMY_STUB("EyeToyClient::ApplyPresetFilter(int)", 0x80038E34); }
void EyeToyClient::ApplyFunFrame(int) { SIMMY_STUB("EyeToyClient::ApplyFunFrame(int)", 0x80038E50); }
void EyeToyClient::WriteTextureToRepSlot(int, ETexture *, EyeToyClient__SlotImageType) { SIMMY_STUB("EyeToyClient::WriteTextureToRepSlot(int, ETexture *, EyeToyClient::SlotImageType)", 0x80038E64); }
void EyeToyClient::SaveSessionToSlot(int) { SIMMY_STUB("EyeToyClient::SaveSessionToSlot(int)", 0x80038EEC); }
RET_UNKNOWN EyeToyClient::GetSessionTexture() { SIMMY_STUB("EyeToyClient::GetSessionTexture(void)", 0x80038F40); return {}; }
RET_UNKNOWN EyeToyClient::StartStream() { SIMMY_STUB("EyeToyClient::StartStream(void)", 0x80038F48); return {}; }
RET_UNKNOWN EyeToyClient::StopStream() { SIMMY_STUB("EyeToyClient::StopStream(void)", 0x80038F7C); return {}; }
bool EyeToyClient::IsEyeToyHardwarePresent() { SIMMY_STUB("EyeToyClient::IsEyeToyHardwarePresent(void)", 0x80038FB0); return {}; }
RET_UNKNOWN EyeToyClient::IsSessionEyeToyDisconnected() { SIMMY_STUB("EyeToyClient::IsSessionEyeToyDisconnected(void)", 0x80038FE0); return {}; }
RET_UNKNOWN EyeToyClient::GetRepSlotUnLockedBits(int) { SIMMY_STUB("EyeToyClient::GetRepSlotUnLockedBits(int)", 0x80039014); return {}; }
RET_UNKNOWN EyeToyClient::GetFunFrameUnLockedBits(int) { SIMMY_STUB("EyeToyClient::GetFunFrameUnLockedBits(int)", 0x800390FC); return {}; }
bool EyeToyClient::DoesRepSlotHaveEyetoyImage(int) { SIMMY_STUB("EyeToyClient::DoesRepSlotHaveEyetoyImage(int)", 0x80039178); return {}; }
RET_UNKNOWN EyeToyClient::GetRepShaderGUID(int) { SIMMY_STUB("EyeToyClient::GetRepShaderGUID(int)", 0x8003919C); return {}; }
bool EyeToyClient::IsSlotOpenForCASImage(int, int) { SIMMY_STUB("EyeToyClient::IsSlotOpenForCASImage(int, int)", 0x800391B4); return {}; }
void EyeToyClient::SetRepTexture(int, ETexture *) { SIMMY_STUB("EyeToyClient::SetRepTexture(int, ETexture *)", 0x800391DC); }
RET_UNKNOWN EyeToyClient::GetRepTexture(int) { SIMMY_STUB("EyeToyClient::GetRepTexture(int)", 0x80039200); return {}; }
RET_UNKNOWN EyeToyClient::GetRepShaderERTextureID(EyeToyClient__RepShaderMapping) { SIMMY_STUB("EyeToyClient::GetRepShaderERTextureID(EyeToyClient::RepShaderMapping)", 0x80039220); return {}; }
void EyeToyClient::RestoreDefaults() { SIMMY_STUB("EyeToyClient::RestoreDefaults(void)", 0x80039240); }
double EyeToyClient::GetExposure() { SIMMY_STUB("EyeToyClient::GetExposure(void)", 0x80039270); return {}; }
void EyeToyClient::SetExposure(float) { SIMMY_STUB("EyeToyClient::SetExposure(float)", 0x800392C4); }
double EyeToyClient::GetSaturation() { SIMMY_STUB("EyeToyClient::GetSaturation(void)", 0x8003932C); return {}; }
void EyeToyClient::SetSaturation(float) { SIMMY_STUB("EyeToyClient::SetSaturation(float)", 0x80039344); }
double EyeToyClient::GetHue() { SIMMY_STUB("EyeToyClient::GetHue(void)", 0x80039378); return {}; }
void EyeToyClient::SetHue(float) { SIMMY_STUB("EyeToyClient::SetHue(float)", 0x800393B0); }
double EyeToyClient::GetBrightness() { SIMMY_STUB("EyeToyClient::GetBrightness(void)", 0x80039434); return {}; }
void EyeToyClient::SetBrightness(float) { SIMMY_STUB("EyeToyClient::SetBrightness(float)", 0x8003944C); }
RET_UNKNOWN EyeToyClient__Debug::GetTierRepShaderCount(int) { SIMMY_STUB("EyeToyClient::Debug::GetTierRepShaderCount(int)", 0x80039480); return {}; }
RET_UNKNOWN EyeToyClient__Debug::GetTierNthRepShader(int, int) { SIMMY_STUB("EyeToyClient::Debug::GetTierNthRepShader(int, int)", 0x80039498); return {}; }
void EyeToyClient__Debug::WriteTGAToHost(ETexture *) { SIMMY_STUB("EyeToyClient::Debug::WriteTGAToHost(ETexture *)", 0x800394B8); }
void EyeToyClient__Debug::WriteTGAToHost(short *, int, int) { SIMMY_STUB("EyeToyClient::Debug::WriteTGAToHost(short *, int, int)", 0x80039510); }
