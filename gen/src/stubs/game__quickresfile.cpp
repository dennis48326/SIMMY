// GENERATED stubs for unported functions of game/quickresfile.cpp. Do not edit.
#include "simmy/types_gen.hpp"

QuickResFile::QuickResFile() { SIMMY_STUB("QuickResFile::QuickResFile(void)", 0x801329B4); }
QuickResFile::~QuickResFile() { SIMMY_STUB("QuickResFile::~QuickResFile(void)", 0x801329F0); }
RET_UNKNOWN QuickResFile::Create(StringBuffer &) { SIMMY_STUB("QuickResFile::Create(StringBuffer &)", 0x80132A54); return {}; }
RET_UNKNOWN QuickResFile::Delete(StringBuffer &) { SIMMY_STUB("QuickResFile::Delete(StringBuffer &)", 0x80132A5C); return {}; }
RET_UNKNOWN QuickResFile::Open(StringBuffer &) { SIMMY_STUB("QuickResFile::Open(StringBuffer &)", 0x80132A64); return {}; }
RET_UNKNOWN QuickResFile::CloseForReopen() { SIMMY_STUB("QuickResFile::CloseForReopen(void)", 0x80132AA4); return {}; }
RET_UNKNOWN QuickResFile::Reopen() { SIMMY_STUB("QuickResFile::Reopen(void)", 0x80132AAC); return {}; }
int QuickResFile::Close() { SIMMY_STUB("QuickResFile::Close(void)", 0x80132AB4); return {}; }
void QuickResFile::Update() { SIMMY_STUB("QuickResFile::Update(void)", 0x80132ABC); }
RET_UNKNOWN QuickResFile::Writable() { SIMMY_STUB("QuickResFile::Writable(void)", 0x80132AC0); return {}; }
void QuickResFile::GetFileName(StringBuffer &) { SIMMY_STUB("QuickResFile::GetFileName(StringBuffer &)", 0x80132AC8); }
RET_UNKNOWN QuickResFile::ValidFile() { SIMMY_STUB("QuickResFile::ValidFile(void)", 0x80132B70); return {}; }
RET_UNKNOWN QuickResFile::CountTypes() { SIMMY_STUB("QuickResFile::CountTypes(void)", 0x80132B88); return {}; }
RET_UNKNOWN QuickResFile::GetIndType(short) { SIMMY_STUB("QuickResFile::GetIndType(short)", 0x80132B90); return {}; }
int QuickResFile::Count(int) { SIMMY_STUB("QuickResFile::Count(int)", 0x80132B98); return {}; }
Memory__HandleNode* QuickResFile::GetByID(int, short, void (*)(void *, int)) { SIMMY_STUB("QuickResFile::GetByID(int, short, void (*)(void *, int))", 0x80132BA0); return {}; }
RET_UNKNOWN QuickResFile::GetByName(int, StringBuffer &, void (*)(void *, int)) { SIMMY_STUB("QuickResFile::GetByName(int, StringBuffer &, void (*)(void *, int))", 0x80132BA8); return {}; }
Memory__HandleNode* QuickResFile::GetByIndex(int, short, void (*)(void *, int)) { SIMMY_STUB("QuickResFile::GetByIndex(int, short, void (*)(void *, int))", 0x80132BB0); return {}; }
RET_UNKNOWN QuickResFile::GetByIDAndLanguage(int, short, char, void (*)(void *, int)) { SIMMY_STUB("QuickResFile::GetByIDAndLanguage(int, short, char, void (*)(void *, int))", 0x80132BB8); return {}; }
void QuickResFile::GetName(Memory__HandleNode *, StringBuffer &) { SIMMY_STUB("QuickResFile::GetName(Memory::HandleNode *, StringBuffer &)", 0x80132BC0); }
RET_UNKNOWN QuickResFile::GetResType(Memory__HandleNode *) { SIMMY_STUB("QuickResFile::GetResType(Memory::HandleNode *)", 0x80132BE4); return {}; }
void QuickResFile::GetID(Memory__HandleNode *, short *) { SIMMY_STUB("QuickResFile::GetID(Memory::HandleNode *, short *)", 0x80132BEC); }
void QuickResFile::GetIndex(Memory__HandleNode *, short *) { SIMMY_STUB("QuickResFile::GetIndex(Memory::HandleNode *, short *)", 0x80132BF8); }
RET_UNKNOWN QuickResFile::GetLanguage(Memory__HandleNode *) { SIMMY_STUB("QuickResFile::GetLanguage(Memory::HandleNode *)", 0x80132C04); return {}; }
void QuickResFile::FindUniqueName(int, StringBuffer &) { SIMMY_STUB("QuickResFile::FindUniqueName(int, StringBuffer &)", 0x80132C0C); }
RET_UNKNOWN QuickResFile::FindUniqueID(int) { SIMMY_STUB("QuickResFile::FindUniqueID(int)", 0x80132C30); return {}; }
void QuickResFile::Detach(Memory__HandleNode *) { SIMMY_STUB("QuickResFile::Detach(Memory::HandleNode *)", 0x80132C38); }
void QuickResFile::Load(Memory__HandleNode *) { SIMMY_STUB("QuickResFile::Load(Memory::HandleNode *)", 0x80132C3C); }
int QuickResFile::IsLittleEndian(Memory__HandleNode *) { SIMMY_STUB("QuickResFile::IsLittleEndian(Memory::HandleNode *)", 0x80132C40); return {}; }
void QuickResFile::SetID(Memory__HandleNode *, short) { SIMMY_STUB("QuickResFile::SetID(Memory::HandleNode *, short)", 0x80132C48); }
int QuickResFile::Add(Memory__HandleNode *, int, short, StringBuffer &, bool) { SIMMY_STUB("QuickResFile::Add(Memory::HandleNode *, int, short, StringBuffer &, bool)", 0x80132C4C); return {}; }
RET_UNKNOWN QuickResFile::AddWithLanguage(Memory__HandleNode *, int, short, StringBuffer &, char, bool) { SIMMY_STUB("QuickResFile::AddWithLanguage(Memory::HandleNode *, int, short, StringBuffer &, char, bool)", 0x80132C50); return {}; }
void QuickResFile::Write(Memory__HandleNode *) { SIMMY_STUB("QuickResFile::Write(Memory::HandleNode *)", 0x80132C54); }
void QuickResFile::Remove(Memory__HandleNode *) { SIMMY_STUB("QuickResFile::Remove(Memory::HandleNode *)", 0x80132C58); }
void QuickResFile::SetInfo(Memory__HandleNode *, short, StringBuffer &, char) { SIMMY_STUB("QuickResFile::SetInfo(Memory::HandleNode *, short, StringBuffer &, char)", 0x80132C5C); }
void QuickResFile::GetString(StringBuffer &, short, short) { SIMMY_STUB("QuickResFile::GetString(StringBuffer &, short, short)", 0x80132C60); }
RET_UNKNOWN QuickResFile::_dyncastimpl(SCID) { SIMMY_STUB("QuickResFile::_dyncastimpl(SCID)", 0x80132D1C); return {}; }
