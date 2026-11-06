// GENERATED stubs for unported functions of engine/e_rshader.cpp. Do not edit.
#include "simmy/types_gen.hpp"

RET_UNKNOWN operator>>(EStream &, ERShader *&) { SIMMY_STUB("operator>>(EStream &, ERShader *&)", 0x8031C65C); return {}; }
RET_UNKNOWN operator>>(EFile &, ERShader *&) { SIMMY_STUB("operator>>(EFile &, ERShader *&)", 0x8031C69C); return {}; }
void ERShader::OldLoad(EFile &, EShaderDef &) { SIMMY_STUB("ERShader::OldLoad(EFile &, EShaderDef &)", 0x8031E838); }
RET_UNKNOWN ERShader::CopyIntermediateShedData(EShaderDef &, char *) { SIMMY_STUB("ERShader::CopyIntermediateShedData(EShaderDef &, char *)", 0x8031EE98); return {}; }
void ERShader::SwapTexture(ERTexture *, int) { SIMMY_STUB("ERShader::SwapTexture(ERTexture *, int)", 0x80320220); }
RET_UNKNOWN ERShader::HasTexture(unsigned int) { SIMMY_STUB("ERShader::HasTexture(unsigned int)", 0x8032031C); return {}; }
void ERShader::RegisterType(unsigned short) { SIMMY_STUB("ERShader::RegisterType(unsigned short)", 0x8032057C); }
void ERShader::CreateCopy() const { SIMMY_STUB("ERShader::CreateCopy(void) const", 0x803205D0); }
void ERShader::SetSupportDecal(bool) { SIMMY_STUB("ERShader::SetSupportDecal(bool)", 0x80320680); }
RET_UNKNOWN ERShader::GetSupportDecal() { SIMMY_STUB("ERShader::GetSupportDecal(void)", 0x80320708); return {}; }
RET_UNKNOWN ERShader::IsDayNightShader() { SIMMY_STUB("ERShader::IsDayNightShader(void)", 0x80320750); return {}; }
RET_UNKNOWN ERShader::GetRTextureForRenderPass(int) { SIMMY_STUB("ERShader::GetRTextureForRenderPass(int)", 0x8032075C); return {}; }
RET_UNKNOWN ERShader::GetCurrentShader() { SIMMY_STUB("ERShader::GetCurrentShader(void)", 0x8032076C); return {}; }
bool ERShader::SupportsMultiShaderStates() { SIMMY_STUB("ERShader::SupportsMultiShaderStates(void)", 0x803207A8); return {}; }
RET_UNKNOWN ERShader::GetStateCount() { SIMMY_STUB("ERShader::GetStateCount(void)", 0x803207BC); return {}; }
RET_UNKNOWN ERShader::GetState(unsigned int) { SIMMY_STUB("ERShader::GetState(unsigned int)", 0x803207F8); return {}; }
RET_UNKNOWN ERShader::GetCurrentGraphicsState() { SIMMY_STUB("ERShader::GetCurrentGraphicsState(void)", 0x8032084C); return {}; }
RET_UNKNOWN ERShader::IsMultiShader() const { SIMMY_STUB("ERShader::IsMultiShader(void) const", 0x80320860); return {}; }
RET_UNKNOWN ERShader::IsMultiTextureShader() { SIMMY_STUB("ERShader::IsMultiTextureShader(void)", 0x8032086C); return {}; }
void ERShader::DetachClone() { SIMMY_STUB("ERShader::DetachClone(void)", 0x803208B4); }
RET_UNKNOWN ERShader::GetTimeState() { SIMMY_STUB("ERShader::GetTimeState(void)", 0x803208DC); return {}; }
void ERShader::Select(ERC *, unsigned int) { SIMMY_STUB("ERShader::Select(ERC *, unsigned int)", 0x80320904); }
void ERShader::SelectForShadowMask(ERC *) { SIMMY_STUB("ERShader::SelectForShadowMask(ERC *)", 0x80320964); }
RET_UNKNOWN ERShader::CanColorBeModified() { SIMMY_STUB("ERShader::CanColorBeModified(void)", 0x803209C4); return {}; }
RET_UNKNOWN ERShader::GetModifiableColor(unsigned int) { SIMMY_STUB("ERShader::GetModifiableColor(unsigned int)", 0x80320A00); return {}; }
