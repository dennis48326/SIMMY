// GENERATED stubs for unported functions of game/nghresfile.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void GetTotalMemoryCardCacheSize() { SIMMY_STUB("GetTotalMemoryCardCacheSize(void)", 0x800D2670); }
void GetMemoryCardCacheSectionSize(unsigned int) { SIMMY_STUB("GetMemoryCardCacheSectionSize(unsigned int)", 0x800D26A0); }
RET_UNKNOWN GetNghIndex(int) { SIMMY_STUB("GetNghIndex(int)", 0x800D3880); return {}; }
RET_UNKNOWN GetHouseIndex(int) { SIMMY_STUB("GetHouseIndex(int)", 0x800D38B8); return {}; }
RET_UNKNOWN GetUserIndex(int) { SIMMY_STUB("GetUserIndex(int)", 0x800D38F0); return {}; }
RET_UNKNOWN NghResFile::Create(StringBuffer &) { SIMMY_STUB("NghResFile::Create(StringBuffer &)", 0x800D3B94); return {}; }
RET_UNKNOWN NghResFile::Delete(StringBuffer &) { SIMMY_STUB("NghResFile::Delete(StringBuffer &)", 0x800D3BB8); return {}; }
RET_UNKNOWN NghResFile::Open(StringBuffer &) { SIMMY_STUB("NghResFile::Open(StringBuffer &)", 0x800D3BC0); return {}; }
RET_UNKNOWN NghResFile::CloseForReopen() { SIMMY_STUB("NghResFile::CloseForReopen(void)", 0x800D3BC8); return {}; }
RET_UNKNOWN NghResFile::Reopen() { SIMMY_STUB("NghResFile::Reopen(void)", 0x800D3BD0); return {}; }
void NghResFile::Update() { SIMMY_STUB("NghResFile::Update(void)", 0x800D3BE0); }
RET_UNKNOWN NghResFile::Writable() { SIMMY_STUB("NghResFile::Writable(void)", 0x800D3BE4); return {}; }
void NghResFile::GetFileName(StringBuffer &) { SIMMY_STUB("NghResFile::GetFileName(StringBuffer &)", 0x800D3BEC); }
RET_UNKNOWN NghResFile::ValidFile() { SIMMY_STUB("NghResFile::ValidFile(void)", 0x800D3C18); return {}; }
RET_UNKNOWN NghResFile::CountTypes() { SIMMY_STUB("NghResFile::CountTypes(void)", 0x800D3C20); return {}; }
RET_UNKNOWN NghResFile::GetIndType(short) { SIMMY_STUB("NghResFile::GetIndType(short)", 0x800D3C28); return {}; }
RET_UNKNOWN NghResFile::GetByName(int, StringBuffer &, void (*)(void *, int)) { SIMMY_STUB("NghResFile::GetByName(int, StringBuffer &, void (*)(void *, int))", 0x800D3CC8); return {}; }
void NghResFile::GetName(Memory__HandleNode *, StringBuffer &) { SIMMY_STUB("NghResFile::GetName(Memory::HandleNode *, StringBuffer &)", 0x800D3D3C); }
RET_UNKNOWN NghResFile::GetResType(Memory__HandleNode *) { SIMMY_STUB("NghResFile::GetResType(Memory::HandleNode *)", 0x800D3D40); return {}; }
void NghResFile::GetIndex(Memory__HandleNode *, short *) { SIMMY_STUB("NghResFile::GetIndex(Memory::HandleNode *, short *)", 0x800D3D58); }
void NghResFile::FindUniqueName(int, StringBuffer &) { SIMMY_STUB("NghResFile::FindUniqueName(int, StringBuffer &)", 0x800D3D5C); }
RET_UNKNOWN NghResFile::FindUniqueID(int) { SIMMY_STUB("NghResFile::FindUniqueID(int)", 0x800D3D60); return {}; }
void NghResFile::Detach(Memory__HandleNode *) { SIMMY_STUB("NghResFile::Detach(Memory::HandleNode *)", 0x800D3D68); }
void NghResFile::Load(Memory__HandleNode *) { SIMMY_STUB("NghResFile::Load(Memory::HandleNode *)", 0x800D3D6C); }
void NghResFile::SetID(Memory__HandleNode *, short) { SIMMY_STUB("NghResFile::SetID(Memory::HandleNode *, short)", 0x800D3D78); }
void NghResFile::Write(Memory__HandleNode *) { SIMMY_STUB("NghResFile::Write(Memory::HandleNode *)", 0x800D3F6C); }
void NghResFile::Remove(Memory__HandleNode *) { SIMMY_STUB("NghResFile::Remove(Memory::HandleNode *)", 0x800D3F70); }
void NghResFile::SetInfo(Memory__HandleNode *, short, StringBuffer &, char) { SIMMY_STUB("NghResFile::SetInfo(Memory::HandleNode *, short, StringBuffer &, char)", 0x800D3F74); }
void NghResFile::reset() { SIMMY_STUB("NghResFile::reset(void)", 0x800D4258); }
RET_UNKNOWN NghResFile::WriteToMemoryBlock(void *&, unsigned int &) { SIMMY_STUB("NghResFile::WriteToMemoryBlock(void *&, unsigned int &)", 0x800D60A0); return {}; }
RET_UNKNOWN NghResFile::XBoxReadFromMemoryBlock(void *, unsigned int) { SIMMY_STUB("NghResFile::XBoxReadFromMemoryBlock(void *, unsigned int)", 0x800D6204); return {}; }
RET_UNKNOWN NghResFile::WriteToFile(char *) { SIMMY_STUB("NghResFile::WriteToFile(char *)", 0x800D6FF4); return {}; }
RET_UNKNOWN NghResFile::WriteToFileInMemoryBlocks(char *) { SIMMY_STUB("NghResFile::WriteToFileInMemoryBlocks(char *)", 0x800D70A4); return {}; }
RET_UNKNOWN NghResFile::CopyDefaultNghToCard(char *, char *, unsigned char) { SIMMY_STUB("NghResFile::CopyDefaultNghToCard(char *, char *, unsigned char)", 0x800D8330); return {}; }
RET_UNKNOWN NghResFile::CopyHouse(int, NghResFile &, int) { SIMMY_STUB("NghResFile::CopyHouse(int, NghResFile &, int)", 0x800D86E8); return {}; }
void NghResFile::GetDefaultNghPath(char *, StackString<260> &) { SIMMY_STUB("NghResFile::GetDefaultNghPath(char *, StackString<260> &)", 0x800D8954); }
