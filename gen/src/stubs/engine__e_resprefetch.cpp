// GENERATED stubs for unported functions of engine/e_resprefetch.cpp. Do not edit.
#include "simmy/types_gen.hpp"

EResPrefetch::EResPrefetch() { SIMMY_STUB("EResPrefetch::EResPrefetch(void)", 0x80312380); }
EResPrefetch::~EResPrefetch() { SIMMY_STUB("EResPrefetch::~EResPrefetch(void)", 0x8031240C); }
RET_UNKNOWN EResPrefetch::Init() { SIMMY_STUB("EResPrefetch::Init(void)", 0x803124B0); return {}; }
void EResPrefetch::Shutdown() { SIMMY_STUB("EResPrefetch::Shutdown(void)", 0x8031257C); }
void EResPrefetch::AcquireBuffer(void *, unsigned int) { SIMMY_STUB("EResPrefetch::AcquireBuffer(void *, unsigned int)", 0x803125C0); }
void EResPrefetch::ReleasedBuffer(EScratchBuffUser *) { SIMMY_STUB("EResPrefetch::ReleasedBuffer(EScratchBuffUser *)", 0x803125D4); }
void EResPrefetch::DoCancel(EPrefetchMsg *) { SIMMY_STUB("EResPrefetch::DoCancel(EPrefetchMsg *)", 0x80312648); }
void EResPrefetch::DoBegin(EPrefetchMsg *) { SIMMY_STUB("EResPrefetch::DoBegin(EPrefetchMsg *)", 0x803126D4); }
void EResPrefetch::LoadAnotherBlock() { SIMMY_STUB("EResPrefetch::LoadAnotherBlock(void)", 0x80312790); }
void EResPrefetch::DoFlush(EPrefetchMsg *) { SIMMY_STUB("EResPrefetch::DoFlush(EPrefetchMsg *)", 0x80312984); }
void EResPrefetch::Main() { SIMMY_STUB("EResPrefetch::Main(void)", 0x803129D8); }
void EResPrefetch::Flush() { SIMMY_STUB("EResPrefetch::Flush(void)", 0x80312A70); }
void EResPrefetch::Cancel() { SIMMY_STUB("EResPrefetch::Cancel(void)", 0x80312B10); }
void EResPrefetch::BeginPreFetch(char *, char *) { SIMMY_STUB("EResPrefetch::BeginPreFetch(char *, char *)", 0x80312BA0); }
void EResPrefetch::BeginPreFetch(char *, unsigned int) { SIMMY_STUB("EResPrefetch::BeginPreFetch(char *, unsigned int)", 0x80312BE8); }
void EResPrefetch::AddRef(char *, char *) { SIMMY_STUB("EResPrefetch::AddRef(char *, char *)", 0x80312C70); }
RET_UNKNOWN EResPrefetch::AddRef(char *, unsigned int) { SIMMY_STUB("EResPrefetch::AddRef(char *, unsigned int)", 0x80312CB8); return {}; }
RET_UNKNOWN EResPrefetch::CanAddRef() { SIMMY_STUB("EResPrefetch::CanAddRef(void)", 0x80312DB0); return {}; }
RET_UNKNOWN EResPrefetch::CanReleaseBuffer() { SIMMY_STUB("EResPrefetch::CanReleaseBuffer(void)", 0x80312E68); return {}; }
double EResPrefetch::GetLoadProgress() { SIMMY_STUB("EResPrefetch::GetLoadProgress(void)", 0x80312EC4); return {}; }
void EResPrefetch::ResetState() { SIMMY_STUB("EResPrefetch::ResetState(void)", 0x80312EE4); }
