// GENERATED stubs for unported functions of libaptz/AptLoad.cpp.cpp. Do not edit.
#include "simmy/types_gen.hpp"

RET_UNKNOWN AptFileNameCompare(char *, char *) { SIMMY_STUB("AptFileNameCompare(char *, char *)", 0x8029E270); return {}; }
RET_UNKNOWN AptLoader::findFile(EAStringC &) { SIMMY_STUB("AptLoader::findFile(EAStringC &)", 0x8029E360); return {}; }
AptLoader::~AptLoader() { SIMMY_STUB("AptLoader::~AptLoader(void)", 0x8029E408); }
void AptLoader::Invalidate(AptFile *) { SIMMY_STUB("AptLoader::Invalidate(AptFile *)", 0x8029E474); }
void AptLoader::notify(AptSharedPtr<AptFile>) { SIMMY_STUB("AptLoader::notify(AptSharedPtr<AptFile>)", 0x8029E578); }
RET_UNKNOWN AptLoader::IsLoaded(EAStringC &) { SIMMY_STUB("AptLoader::IsLoaded(EAStringC &)", 0x8029E5E0); return {}; }
RET_UNKNOWN AptLoader::Load(EAStringC &) { SIMMY_STUB("AptLoader::Load(EAStringC &)", 0x8029E69C); return {}; }
RET_UNKNOWN AptLoader::AllImportsAvailable(AptSharedPtr<AptFile>) { SIMMY_STUB("AptLoader::AllImportsAvailable(AptSharedPtr<AptFile>)", 0x8029E7E0); return {}; }
void AptLoader::Update() { SIMMY_STUB("AptLoader::Update(void)", 0x8029E900); }
void AptLoader::CompleteLoad(AptSharedPtr<AptFile>, void *, void *, void *) { SIMMY_STUB("AptLoader::CompleteLoad(AptSharedPtr<AptFile>, void *, void *, void *)", 0x8029EA94); }
RET_UNKNOWN AptLoader::CancelPreloadedAnimation(EAStringC &) { SIMMY_STUB("AptLoader::CancelPreloadedAnimation(EAStringC &)", 0x8029EB60); return {}; }
void AptLinker::SwapOut(AptSharedPtr<AptFile>, AptSharedPtr<AptFile>) { SIMMY_STUB("AptLinker::SwapOut(AptSharedPtr<AptFile>, AptSharedPtr<AptFile>)", 0x8029EE74); }
void AptLinker::Update() { SIMMY_STUB("AptLinker::Update(void)", 0x8029F2A8); }
void AptLinker::Notify(AptSharedPtr<AptFile>) { SIMMY_STUB("AptLinker::Notify(AptSharedPtr<AptFile>)", 0x8029FCDC); }
void AptLinker::Load(EAStringC &, EAStringC) { SIMMY_STUB("AptLinker::Load(EAStringC &, EAStringC)", 0x802A040C); }
void AptLinker::CancelLoad(AptCIH *) { SIMMY_STUB("AptLinker::CancelLoad(AptCIH *)", 0x802A0E80); }
void AptSharedPtrIncRef(AptFile *) { SIMMY_STUB("AptSharedPtrIncRef(AptFile *)", 0x802A0FE8); }
void AptSharedPtrDecRef(AptFile *) { SIMMY_STUB("AptSharedPtrDecRef(AptFile *)", 0x802A0FFC); }
void AptSharedPtrDelete(AptFile *) { SIMMY_STUB("AptSharedPtrDelete(AptFile *)", 0x802A1010); }
void AptSharedPtrIncRef(AptLinkerThingy *) { SIMMY_STUB("AptSharedPtrIncRef(AptLinkerThingy *)", 0x802A1094); }
void AptSharedPtrDecRef(AptLinkerThingy *) { SIMMY_STUB("AptSharedPtrDecRef(AptLinkerThingy *)", 0x802A10A8); }
void AptSharedPtrDelete(AptLinkerThingy *) { SIMMY_STUB("AptSharedPtrDelete(AptLinkerThingy *)", 0x802A10BC); }
void GlobalNotificationFunction(AptSharedPtr<AptFile>) { SIMMY_STUB("GlobalNotificationFunction(AptSharedPtr<AptFile>)", 0x802A1118); }
RET_UNKNOWN AptFile::GetMainCharacter() const { SIMMY_STUB("AptFile::GetMainCharacter(void) const", 0x802A17F0); return {}; }
