// GENERATED stubs for unported functions of game/iffresfile2.cpp. Do not edit.
#include "simmy/types_gen.hpp"

RET_UNKNOWN GetLanguage(short) { SIMMY_STUB("GetLanguage(short)", 0x800BCD04); return {}; }
void SetLanguage(short *, char) { SIMMY_STUB("SetLanguage(short *, char)", 0x800BCD0C); }
void IFFResNode::DoStream(ReconBuffer *, int) { SIMMY_STUB("IFFResNode::DoStream(ReconBuffer *, int)", 0x800BCD20); }
void IFFResList::DoStream(ReconBuffer *, int) { SIMMY_STUB("IFFResList::DoStream(ReconBuffer *, int)", 0x800BCDAC); }
RET_UNKNOWN IFFResMap::UpdateOffset(int, int) { SIMMY_STUB("IFFResMap::UpdateOffset(int, int)", 0x800BCE10); return {}; }
RET_UNKNOWN IFFResMap::GetResList(unsigned int, bool) { SIMMY_STUB("IFFResMap::GetResList(unsigned int, bool)", 0x800BCE80); return {}; }
RET_UNKNOWN IFFResMap::MakeNewNode(unsigned int) { SIMMY_STUB("IFFResMap::MakeNewNode(unsigned int)", 0x800BD15C); return {}; }
RET_UNKNOWN IFFResMap::GetNode(unsigned int, int, void (*)(void *, int)) { SIMMY_STUB("IFFResMap::GetNode(unsigned int, int, void (*)(void *, int))", 0x800BD474); return {}; }
RET_UNKNOWN IFFResMap::GetNodeWithLanguage(unsigned int, int, char, void (*)(void *, int)) { SIMMY_STUB("IFFResMap::GetNodeWithLanguage(unsigned int, int, char, void (*)(void *, int))", 0x800BD514); return {}; }
RET_UNKNOWN IFFResMap::GetNode(unsigned int, StackString<64> &, void (*)(void *, int)) { SIMMY_STUB("IFFResMap::GetNode(unsigned int, StackString<64> &, void (*)(void *, int))", 0x800BD5FC); return {}; }
RET_UNKNOWN IFFResMap::GetNode(Memory__HandleNode *, unsigned int *, void (*)(void *, int)) { SIMMY_STUB("IFFResMap::GetNode(Memory::HandleNode *, unsigned int *, void (*)(void *, int))", 0x800BD6A4); return {}; }
RET_UNKNOWN IFFResMap::GetIndNode(unsigned int, int, void (*)(void *, int)) { SIMMY_STUB("IFFResMap::GetIndNode(unsigned int, int, void (*)(void *, int))", 0x800BD71C); return {}; }
void IFFResMap::RemoveNode(IFFResNode *) { SIMMY_STUB("IFFResMap::RemoveNode(IFFResNode *)", 0x800BD7AC); }
RET_UNKNOWN IFFResMap::CountTypes() { SIMMY_STUB("IFFResMap::CountTypes(void)", 0x800BD968); return {}; }
RET_UNKNOWN IFFResMap::GetIndexedType(int) { SIMMY_STUB("IFFResMap::GetIndexedType(int)", 0x800BD988); return {}; }
RET_UNKNOWN IFFResMap::CountNodes(unsigned int) { SIMMY_STUB("IFFResMap::CountNodes(unsigned int)", 0x800BD9CC); return {}; }
void IFFResMap::FreeAllHandles() { SIMMY_STUB("IFFResMap::FreeAllHandles(void)", 0x800BDA2C); }
void IFFResMap::RemoveAllNodes() { SIMMY_STUB("IFFResMap::RemoveAllNodes(void)", 0x800BDAC8); }
void IFFResMap::RemoveAllNodesOfType(unsigned int) { SIMMY_STUB("IFFResMap::RemoveAllNodesOfType(unsigned int)", 0x800BDB70); }
RET_UNKNOWN IFFResMap::GetLastSwizzleProc(unsigned int) { SIMMY_STUB("IFFResMap::GetLastSwizzleProc(unsigned int)", 0x800BDC68); return {}; }
RET_UNKNOWN IFFResMap::GetHighestID(unsigned int, short) { SIMMY_STUB("IFFResMap::GetHighestID(unsigned int, short)", 0x800BDCB0); return {}; }
void IFFResMap::DoStream(ReconBuffer *, int) { SIMMY_STUB("IFFResMap::DoStream(ReconBuffer *, int)", 0x800BDD24); }
IFFResFile2::IFFResFile2() { SIMMY_STUB("IFFResFile2::IFFResFile2(void)", 0x800BDD78); }
IFFResFile2::~IFFResFile2() { SIMMY_STUB("IFFResFile2::~IFFResFile2(void)", 0x800BDDE8); }
RET_UNKNOWN IFFResFile2::Open(StringBuffer &) { SIMMY_STUB("IFFResFile2::Open(StringBuffer &)", 0x800BDE58); return {}; }
RET_UNKNOWN IFFResFile2::Create(StringBuffer &) { SIMMY_STUB("IFFResFile2::Create(StringBuffer &)", 0x800BE264); return {}; }
RET_UNKNOWN IFFResFile2::ClearMap(StringBuffer &) { SIMMY_STUB("IFFResFile2::ClearMap(StringBuffer &)", 0x800BE310); return {}; }
RET_UNKNOWN IFFResFile2::WriteHeader(MemFile *, int) { SIMMY_STUB("IFFResFile2::WriteHeader(MemFile *, int)", 0x800BE3A0); return {}; }
RET_UNKNOWN IFFResFile2::Delete(StringBuffer &) { SIMMY_STUB("IFFResFile2::Delete(StringBuffer &)", 0x800BE4A4); return {}; }
int IFFResFile2::Close() { SIMMY_STUB("IFFResFile2::Close(void)", 0x800BE504); return {}; }
RET_UNKNOWN IFFResFile2::CloseForReopen() { SIMMY_STUB("IFFResFile2::CloseForReopen(void)", 0x800BE7CC); return {}; }
RET_UNKNOWN IFFResFile2::Reopen() { SIMMY_STUB("IFFResFile2::Reopen(void)", 0x800BE834); return {}; }
void IFFResFile2::Update() { SIMMY_STUB("IFFResFile2::Update(void)", 0x800BE8CC); }
RET_UNKNOWN IFFResFile2::Writable() { SIMMY_STUB("IFFResFile2::Writable(void)", 0x800BEA00); return {}; }
void IFFResFile2::GetFileName(StringBuffer &) { SIMMY_STUB("IFFResFile2::GetFileName(StringBuffer &)", 0x800BEA08); }
RET_UNKNOWN IFFResFile2::ValidFile() { SIMMY_STUB("IFFResFile2::ValidFile(void)", 0x800BEA2C); return {}; }
RET_UNKNOWN IFFResFile2::CountTypes() { SIMMY_STUB("IFFResFile2::CountTypes(void)", 0x800BEA50); return {}; }
RET_UNKNOWN IFFResFile2::GetIndType(short) { SIMMY_STUB("IFFResFile2::GetIndType(short)", 0x800BEAD8); return {}; }
int IFFResFile2::Count(int) { SIMMY_STUB("IFFResFile2::Count(int)", 0x800BEB78); return {}; }
RET_UNKNOWN IFFResFile2::GetByIDAndLanguage(int, short, char, void (*)(void *, int)) { SIMMY_STUB("IFFResFile2::GetByIDAndLanguage(int, short, char, void (*)(void *, int))", 0x800BEC08); return {}; }
Memory__HandleNode* IFFResFile2::GetByID(int, short, void (*)(void *, int)) { SIMMY_STUB("IFFResFile2::GetByID(int, short, void (*)(void *, int))", 0x800BED18); return {}; }
RET_UNKNOWN IFFResFile2::GetByName(int, StringBuffer &, void (*)(void *, int)) { SIMMY_STUB("IFFResFile2::GetByName(int, StringBuffer &, void (*)(void *, int))", 0x800BED54); return {}; }
Memory__HandleNode* IFFResFile2::GetByIndex(int, short, void (*)(void *, int)) { SIMMY_STUB("IFFResFile2::GetByIndex(int, short, void (*)(void *, int))", 0x800BEE64); return {}; }
void IFFResFile2::GetName(Memory__HandleNode *, StringBuffer &) { SIMMY_STUB("IFFResFile2::GetName(Memory::HandleNode *, StringBuffer &)", 0x800BEF6C); }
RET_UNKNOWN IFFResFile2::GetLanguage(Memory__HandleNode *) { SIMMY_STUB("IFFResFile2::GetLanguage(Memory::HandleNode *)", 0x800BF028); return {}; }
RET_UNKNOWN IFFResFile2::GetResType(Memory__HandleNode *) { SIMMY_STUB("IFFResFile2::GetResType(Memory::HandleNode *)", 0x800BF0DC); return {}; }
void IFFResFile2::GetID(Memory__HandleNode *, short *) { SIMMY_STUB("IFFResFile2::GetID(Memory::HandleNode *, short *)", 0x800BF18C); }
void IFFResFile2::GetIndex(Memory__HandleNode *, short *) { SIMMY_STUB("IFFResFile2::GetIndex(Memory::HandleNode *, short *)", 0x800BF23C); }
void IFFResFile2::FindUniqueName(int, StringBuffer &) { SIMMY_STUB("IFFResFile2::FindUniqueName(int, StringBuffer &)", 0x800BF2C4); }
RET_UNKNOWN IFFResFile2::FindUniqueID(int) { SIMMY_STUB("IFFResFile2::FindUniqueID(int)", 0x800BF3D4); return {}; }
void IFFResFile2::Detach(Memory__HandleNode *) { SIMMY_STUB("IFFResFile2::Detach(Memory::HandleNode *)", 0x800BF46C); }
void IFFResFile2::Load(Memory__HandleNode *) { SIMMY_STUB("IFFResFile2::Load(Memory::HandleNode *)", 0x800BF550); }
int IFFResFile2::IsLittleEndian(Memory__HandleNode *) { SIMMY_STUB("IFFResFile2::IsLittleEndian(Memory::HandleNode *)", 0x800BF5D0); return {}; }
void IFFResFile2::SetID(Memory__HandleNode *, short) { SIMMY_STUB("IFFResFile2::SetID(Memory::HandleNode *, short)", 0x800BF690); }
RET_UNKNOWN IFFResFile2::AddWithLanguage(Memory__HandleNode *, int, short, StringBuffer &, char, bool) { SIMMY_STUB("IFFResFile2::AddWithLanguage(Memory::HandleNode *, int, short, StringBuffer &, char, bool)", 0x800BF744); return {}; }
int IFFResFile2::Add(Memory__HandleNode *, int, short, StringBuffer &, bool) { SIMMY_STUB("IFFResFile2::Add(Memory::HandleNode *, int, short, StringBuffer &, bool)", 0x800BF8E4); return {}; }
void IFFResFile2::Write(Memory__HandleNode *) { SIMMY_STUB("IFFResFile2::Write(Memory::HandleNode *)", 0x800BF920); }
void IFFResFile2::Remove(Memory__HandleNode *) { SIMMY_STUB("IFFResFile2::Remove(Memory::HandleNode *)", 0x800BFB5C); }
void IFFResFile2::SetInfo(Memory__HandleNode *, short, StringBuffer &, char) { SIMMY_STUB("IFFResFile2::SetInfo(Memory::HandleNode *, short, StringBuffer &, char)", 0x800BFC68); }
RET_UNKNOWN IFFResFile2::LoadNode(IFFResNode *, void (*)(void *, int), int) { SIMMY_STUB("IFFResFile2::LoadNode(IFFResNode *, void (*)(void *, int), int)", 0x800BFDD4); return {}; }
RET_UNKNOWN IFFResFile2::LowLevelRemove(IFFResNode *) { SIMMY_STUB("IFFResFile2::LowLevelRemove(IFFResNode *)", 0x800BFF6C); return {}; }
void IFFResFile2::InvalBlockHeader(int) { SIMMY_STUB("IFFResFile2::InvalBlockHeader(int)", 0x800BFFE8); }
RET_UNKNOWN IFFResFile2::NewBlockHeader(IFFHeader *, unsigned int, int *) { SIMMY_STUB("IFFResFile2::NewBlockHeader(IFFHeader *, unsigned int, int *)", 0x800C0064); return {}; }
RET_UNKNOWN IFFResFile2::GetBlockHeader(IFFHeader *, int) { SIMMY_STUB("IFFResFile2::GetBlockHeader(IFFHeader *, int)", 0x800C02B4); return {}; }
void IFFResFile2::SetBlockHeader(IFFHeader *, int) { SIMMY_STUB("IFFResFile2::SetBlockHeader(IFFHeader *, int)", 0x800C0318); }
RET_UNKNOWN IFFResFile2::MoveBlock(unsigned int, unsigned int, unsigned int, unsigned char *) { SIMMY_STUB("IFFResFile2::MoveBlock(unsigned int, unsigned int, unsigned int, unsigned char *)", 0x800C03A0); return {}; }
RET_UNKNOWN IFFResFile2::Defrag() { SIMMY_STUB("IFFResFile2::Defrag(void)", 0x800C047C); return {}; }
RET_UNKNOWN IFFBehResFile::Open(StringBuffer &) { SIMMY_STUB("IFFBehResFile::Open(StringBuffer &)", 0x800C057C); return {}; }
RET_UNKNOWN IFFBehResFile::IFFNodeSize(unsigned short, unsigned int) { SIMMY_STUB("IFFBehResFile::IFFNodeSize(unsigned short, unsigned int)", 0x800C09EC); return {}; }
void IFFBehResFile::RefreshBufferWithBehavior() { SIMMY_STUB("IFFBehResFile::RefreshBufferWithBehavior(void)", 0x800C0A7C); }
void IFFResFile2::SetOptimizeTarget() { SIMMY_STUB("IFFResFile2::SetOptimizeTarget(void)", 0x800C1034); }
