// GENERATED stubs for unported functions of engine/e_ngcrenderer.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void PerfAlloc(unsigned long) { SIMMY_STUB("PerfAlloc(unsigned long)", 0x8033B404); }
void PerfFree(void *) { SIMMY_STUB("PerfFree(void *)", 0x8033B43C); }
void ScreenShotAlloc(unsigned int) { SIMMY_STUB("ScreenShotAlloc(unsigned int)", 0x8033B470); }
void ScreenShotFree(void *) { SIMMY_STUB("ScreenShotFree(void *)", 0x8033B4B0); }
void ClearXFB(unsigned char *, int) { SIMMY_STUB("ClearXFB(unsigned char *, int)", 0x8033B4E4); }
void ENgcRenderer::ShowPerfGraphs(bool) { SIMMY_STUB("ENgcRenderer::ShowPerfGraphs(bool)", 0x8033B720); }
RET_UNKNOWN ENgcRenderer::GetCurrentTexture(int) { SIMMY_STUB("ENgcRenderer::GetCurrentTexture(int)", 0x8033B724); return {}; }
void ENgcRenderer::CreateGXDisplayList(EDL *, GXPrimitive, int, unsigned short *, int, float *, float *, unsigned char *, signed char *, unsigned int &, bool) { SIMMY_STUB("ENgcRenderer::CreateGXDisplayList(EDL *, _GXPrimitive, int, unsigned short *, int, float *, float *, unsigned char *, signed char *, unsigned int &, bool)", 0x8033B764); }
RET_UNKNOWN ENgcRenderer::CreateGXDisplayList(EDL *, GXPrimitive, int, unsigned short *, int, short *, short *, unsigned char *, signed char *, unsigned int &, bool) { SIMMY_STUB("ENgcRenderer::CreateGXDisplayList(EDL *, _GXPrimitive, int, unsigned short *, int, short *, short *, unsigned char *, signed char *, unsigned int &, bool)", 0x8033BBF8); return {}; }
RET_UNKNOWN ENgcRenderer::CreateGXDisplayList(EDL *, GXPrimitive, int, float *, float *, unsigned char *, signed char *, unsigned int &, bool) { SIMMY_STUB("ENgcRenderer::CreateGXDisplayList(EDL *, _GXPrimitive, int, float *, float *, unsigned char *, signed char *, unsigned int &, bool)", 0x8033C08C); return {}; }
RET_UNKNOWN ENgcRenderer::GetExternalFrameBuffer() { SIMMY_STUB("ENgcRenderer::GetExternalFrameBuffer(void)", 0x8033CA98); return {}; }
void ENgcRenderer::CycleToNextFrameBuffer() { SIMMY_STUB("ENgcRenderer::CycleToNextFrameBuffer(void)", 0x8033CAB8); }
void ENgcRenderer::VIPreRetraceCallback(unsigned long) { SIMMY_STUB("ENgcRenderer::VIPreRetraceCallback(unsigned long)", 0x8033CCD4); }
void ENgcRenderer::VIPostRetraceCallback(unsigned long) { SIMMY_STUB("ENgcRenderer::VIPostRetraceCallback(unsigned long)", 0x8033CDEC); }
void ENgcRenderer::BreakPtCallback() { SIMMY_STUB("ENgcRenderer::BreakPtCallback(void)", 0x8033CDF0); }
void ENgcRenderer::DrawSyncCallback(unsigned short) { SIMMY_STUB("ENgcRenderer::DrawSyncCallback(unsigned short)", 0x8033CE20); }
RET_UNKNOWN ENgcRenderer::AddDrawSync() { SIMMY_STUB("ENgcRenderer::AddDrawSync(void)", 0x8033CE28); return {}; }
void VerifyCallback(GXWarningLevel, unsigned long, char *) { SIMMY_STUB("VerifyCallback(GXWarningLevel, unsigned long, char *)", 0x8033CE5C); }
void ENgcRenderer::PushAndClearViewport() { SIMMY_STUB("ENgcRenderer::PushAndClearViewport(void)", 0x8033ECD8); }
void ENgcRenderer::RestoreViewport() { SIMMY_STUB("ENgcRenderer::RestoreViewport(void)", 0x8033EDC8); }
void ENgcRenderer::WeightedBlend(GXPrimitive, int, float *, signed char *, float *, unsigned char *, unsigned char *, bool) { SIMMY_STUB("ENgcRenderer::WeightedBlend(_GXPrimitive, int, float *, signed char *, float *, unsigned char *, unsigned char *, bool)", 0x803424E0); }
void ENgcRenderer::TriStripPacked(int, float *, float *, unsigned char *, signed char *, unsigned char *, unsigned char *, bool) { SIMMY_STUB("ENgcRenderer::TriStripPacked(int, float *, float *, unsigned char *, signed char *, unsigned char *, unsigned char *, bool)", 0x803428D0); }
void ENgcRenderer::TextureMatrix(EDLEntry *) { SIMMY_STUB("ENgcRenderer::TextureMatrix(EDLEntry *)", 0x8034443C); }
void ENgcRenderer::DirectRect(EDLEntry *) { SIMMY_STUB("ENgcRenderer::DirectRect(EDLEntry *)", 0x80344CA0); }
void ENgcRenderer::MipMapSetup(EDLEntry *) { SIMMY_STUB("ENgcRenderer::MipMapSetup(EDLEntry *)", 0x803451A4); }
void ENgcRenderer::Debug(EDLEntry *) { SIMMY_STUB("ENgcRenderer::Debug(EDLEntry *)", 0x803451BC); }
void ENgcRenderer::TriIndexed(EDLEntry *) { SIMMY_STUB("ENgcRenderer::TriIndexed(EDLEntry *)", 0x8034548C); }
void ENgcRenderer::SaveImageData(EDLEntry *) { SIMMY_STUB("ENgcRenderer::SaveImageData(EDLEntry *)", 0x803454A4); }
void ENgcRenderer::SpriteListPacked(EDLEntry *) { SIMMY_STUB("ENgcRenderer::SpriteListPacked(EDLEntry *)", 0x803454BC); }
void ENgcRenderer::SpriteListPacked(int, float *, float *, unsigned char *, signed char *, unsigned char *) { SIMMY_STUB("ENgcRenderer::SpriteListPacked(int, float *, float *, unsigned char *, signed char *, unsigned char *)", 0x803454F8); }
void ENgcRenderer::NgcTexInvalidateAll(EDLEntry *) { SIMMY_STUB("ENgcRenderer::NgcTexInvalidateAll(EDLEntry *)", 0x803477AC); }
void ENgcRenderer::NgcTriListInt(EDLEntry *) { SIMMY_STUB("ENgcRenderer::NgcTriListInt(EDLEntry *)", 0x80347FD8); }
void ENgcRenderer::NgcTriList(EDLEntry *) { SIMMY_STUB("ENgcRenderer::NgcTriList(EDLEntry *)", 0x80348358); }
void ENgcRenderer::NgcScreenTriList(EDLEntry *) { SIMMY_STUB("ENgcRenderer::NgcScreenTriList(EDLEntry *)", 0x80348684); }
void ENgcRenderer::ReadMetrics(EDLEntry *) { SIMMY_STUB("ENgcRenderer::ReadMetrics(EDLEntry *)", 0x803489A8); }
void ENgcRenderer::ScrambleRect(EDLEntry *) { SIMMY_STUB("ENgcRenderer::ScrambleRect(EDLEntry *)", 0x80348DDC); }
RET_UNKNOWN PSMTXMultVecWithScaleAndAdd(float (*)[3], Vec *, float, Vec *) { SIMMY_STUB("PSMTXMultVecWithScaleAndAdd(float (*)[3], Vec *, float, Vec *)", 0x803492B8); return {}; }
RET_UNKNOWN ENgcRenderer::SimpleSkinning(GXPrimitive, int, short *, signed char *, short *, unsigned char *, unsigned char *, bool) { SIMMY_STUB("ENgcRenderer::SimpleSkinning(_GXPrimitive, int, short *, signed char *, short *, unsigned char *, unsigned char *, bool)", 0x80349640); return {}; }
RET_UNKNOWN ENgcRenderer::ScaleTexture(GXTexObj *, void *, int, float) { SIMMY_STUB("ENgcRenderer::ScaleTexture(_GXTexObj *, void *, int, float)", 0x8034991C); return {}; }
RET_UNKNOWN ENgcRenderer::DrawTextureToFullViewportWithJitter(float, float) { SIMMY_STUB("ENgcRenderer::DrawTextureToFullViewportWithJitter(float, float)", 0x80349A70); return {}; }
void ENgcRenderer::DrawFrameWithBloomAndMotionBlur(float, float) { SIMMY_STUB("ENgcRenderer::DrawFrameWithBloomAndMotionBlur(float, float)", 0x80349B94); }
void ENgcRenderer::ProcessThumbnail(void *, int, float, float) { SIMMY_STUB("ENgcRenderer::ProcessThumbnail(void *, int, float, float)", 0x80349CF8); }
RET_UNKNOWN ENgcRenderer::ProcessFrameEffects(float, float, float, float, EVec3 &, EVec3 &) { SIMMY_STUB("ENgcRenderer::ProcessFrameEffects(float, float, float, float, EVec3 &, EVec3 &)", 0x80349F30); return {}; }
RET_UNKNOWN ENgcRenderer::GetRenderMode() { SIMMY_STUB("ENgcRenderer::GetRenderMode(void)", 0x8034A51C); return {}; }
void ENgcRenderer::GetZTestSettings(unsigned char &, unsigned char &, unsigned char &) { SIMMY_STUB("ENgcRenderer::GetZTestSettings(unsigned char &, unsigned char &, unsigned char &)", 0x8034A538); }
RET_UNKNOWN ENgcRenderer::AcquireCopyMutex() { SIMMY_STUB("ENgcRenderer::AcquireCopyMutex(void)", 0x8034A554); return {}; }
void ENgcRenderer::ReleaseCopyMutex() { SIMMY_STUB("ENgcRenderer::ReleaseCopyMutex(void)", 0x8034A594); }
