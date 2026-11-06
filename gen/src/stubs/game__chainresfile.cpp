// GENERATED stubs for unported functions of game/chainresfile.cpp. Do not edit.
#include "simmy/types_gen.hpp"

ChainResFile::ChainResFile() { SIMMY_STUB("ChainResFile::ChainResFile(void)", 0x800B1D94); }
ChainResFile::~ChainResFile() { SIMMY_STUB("ChainResFile::~ChainResFile(void)", 0x800B1E00); }
void ChainResFile::AddFile(iResFile *) { SIMMY_STUB("ChainResFile::AddFile(iResFile *)", 0x800B1E98); }
void ChainResFile::RemoveFile(iResFile *) { SIMMY_STUB("ChainResFile::RemoveFile(iResFile *)", 0x800B1ED8); }
RET_UNKNOWN ChainResFile::CountFiles() { SIMMY_STUB("ChainResFile::CountFiles(void)", 0x800B1FA8); return {}; }
RET_UNKNOWN ChainResFile::GetFile(short) { SIMMY_STUB("ChainResFile::GetFile(short)", 0x800B1FE4); return {}; }
RET_UNKNOWN ChainResFile::Close() { SIMMY_STUB("ChainResFile::Close(void)", 0x800B2044); return {}; }
RET_UNKNOWN ChainResFile::CloseForReopen() { SIMMY_STUB("ChainResFile::CloseForReopen(void)", 0x800B20D8); return {}; }
RET_UNKNOWN ChainResFile::Reopen() { SIMMY_STUB("ChainResFile::Reopen(void)", 0x800B216C); return {}; }
void ChainResFile::Update() { SIMMY_STUB("ChainResFile::Update(void)", 0x800B2200); }
RET_UNKNOWN ChainResFile::Writable() { SIMMY_STUB("ChainResFile::Writable(void)", 0x800B22D4); return {}; }
RET_UNKNOWN ChainResFile::ValidFile() { SIMMY_STUB("ChainResFile::ValidFile(void)", 0x800B2368); return {}; }
RET_UNKNOWN ChainResFile::GetLanguage(Memory__HandleNode *) { SIMMY_STUB("ChainResFile::GetLanguage(Memory::HandleNode *)", 0x800B23FC); return {}; }
RET_UNKNOWN ChainResFile::CountTypes() { SIMMY_STUB("ChainResFile::CountTypes(void)", 0x800B24C4); return {}; }
void ChainResFile::BuildTypeVector(vector<int, allocator<int>> &) { SIMMY_STUB("ChainResFile::BuildTypeVector(vector<int, allocator<int> > &)", 0x800B2558); }
RET_UNKNOWN ChainResFile::GetIndType(short) { SIMMY_STUB("ChainResFile::GetIndType(short)", 0x800B27C4); return {}; }
RET_UNKNOWN ChainResFile::Count(int) { SIMMY_STUB("ChainResFile::Count(int)", 0x800B28B0); return {}; }
void ChainResFile::GetByIDAndLanguage(int, short, char, void (*)(void *, int)) { SIMMY_STUB("ChainResFile::GetByIDAndLanguage(int, short, char, void (*)(void *, int))", 0x800B2970); }
RET_UNKNOWN ChainResFile::GetByID(int, short, void (*)(void *, int)) { SIMMY_STUB("ChainResFile::GetByID(int, short, void (*)(void *, int))", 0x800B2A48); return {}; }
RET_UNKNOWN ChainResFile::GetByName(int, StringBuffer &, void (*)(void *, int)) { SIMMY_STUB("ChainResFile::GetByName(int, StringBuffer &, void (*)(void *, int))", 0x800B2A84); return {}; }
RET_UNKNOWN ChainResFile::GetByIndex(int, short, void (*)(void *, int)) { SIMMY_STUB("ChainResFile::GetByIndex(int, short, void (*)(void *, int))", 0x800B2B54); return {}; }
void ChainResFile::GetName(Memory__HandleNode *, StringBuffer &) { SIMMY_STUB("ChainResFile::GetName(Memory::HandleNode *, StringBuffer &)", 0x800B2C7C); }
RET_UNKNOWN ChainResFile::GetResType(Memory__HandleNode *) { SIMMY_STUB("ChainResFile::GetResType(Memory::HandleNode *)", 0x800B2D40); return {}; }
void ChainResFile::GetID(Memory__HandleNode *, short *) { SIMMY_STUB("ChainResFile::GetID(Memory::HandleNode *, short *)", 0x800B2E08); }
RET_UNKNOWN ChainResFile::IsLittleEndian(Memory__HandleNode *) { SIMMY_STUB("ChainResFile::IsLittleEndian(Memory::HandleNode *)", 0x800B2ECC); return {}; }
void ChainResFile::GetIndex(Memory__HandleNode *, short *) { SIMMY_STUB("ChainResFile::GetIndex(Memory::HandleNode *, short *)", 0x800B2F8C); }
void ChainResFile::FindUniqueName(int, StringBuffer &) { SIMMY_STUB("ChainResFile::FindUniqueName(int, StringBuffer &)", 0x800B3050); }
RET_UNKNOWN ChainResFile::FindUniqueID(int) { SIMMY_STUB("ChainResFile::FindUniqueID(int)", 0x800B3110); return {}; }
void ChainResFile::Detach(Memory__HandleNode *) { SIMMY_STUB("ChainResFile::Detach(Memory::HandleNode *)", 0x800B3174); }
void ChainResFile::Load(Memory__HandleNode *) { SIMMY_STUB("ChainResFile::Load(Memory::HandleNode *)", 0x800B3230); }
void ChainResFile::SetID(Memory__HandleNode *, short) { SIMMY_STUB("ChainResFile::SetID(Memory::HandleNode *, short)", 0x800B3254); }
void ChainResFile::Add(Memory__HandleNode *, int, short, StringBuffer &, bool) { SIMMY_STUB("ChainResFile::Add(Memory::HandleNode *, int, short, StringBuffer &, bool)", 0x800B3318); }
void ChainResFile::AddWithLanguage(Memory__HandleNode *, int, short, StringBuffer &, char, bool) { SIMMY_STUB("ChainResFile::AddWithLanguage(Memory::HandleNode *, int, short, StringBuffer &, char, bool)", 0x800B3354); }
RET_UNKNOWN ChainResFile::Write(Memory__HandleNode *) { SIMMY_STUB("ChainResFile::Write(Memory::HandleNode *)", 0x800B3570); return {}; }
void ChainResFile::Remove(Memory__HandleNode *) { SIMMY_STUB("ChainResFile::Remove(Memory::HandleNode *)", 0x800B364C); }
void ChainResFile::SetInfo(Memory__HandleNode *, short, StringBuffer &, char) { SIMMY_STUB("ChainResFile::SetInfo(Memory::HandleNode *, short, StringBuffer &, char)", 0x800B3728); }
RET_UNKNOWN ChainResFile::AddProhibitedType(iResFile *, unsigned int) { SIMMY_STUB("ChainResFile::AddProhibitedType(iResFile *, unsigned int)", 0x800B381C); return {}; }
void ChainResFile::AddExclusiveType(iResFile *, unsigned int) { SIMMY_STUB("ChainResFile::AddExclusiveType(iResFile *, unsigned int)", 0x800B39CC); }
RET_UNKNOWN ChainResFile::TypeWritable(unsigned int) { SIMMY_STUB("ChainResFile::TypeWritable(unsigned int)", 0x800B3B80); return {}; }
RET_UNKNOWN ChainResFile::_dyncastimpl(SCID) { SIMMY_STUB("ChainResFile::_dyncastimpl(SCID)", 0x800B3C94); return {}; }
