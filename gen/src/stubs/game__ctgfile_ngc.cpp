// GENERATED stubs for unported functions of game/ctgfile_ngc.cpp. Do not edit.
#include "simmy/types_gen.hpp"

CTGFile::~CTGFile() { SIMMY_STUB("CTGFile::~CTGFile(void)", 0x8007CD9C); }
CTGFileImpl::CTGFileImpl(void *, int, int, bool) { SIMMY_STUB("CTGFileImpl::CTGFileImpl(void *, int, int, bool)", 0x8007CDD0); }
CTGFileImpl::~CTGFileImpl() { SIMMY_STUB("CTGFileImpl::~CTGFileImpl(void)", 0x8007CE20); }
RET_UNKNOWN CTGFileImpl::GetSize() { SIMMY_STUB("CTGFileImpl::GetSize(void)", 0x8007CE84); return {}; }
RET_UNKNOWN CTGFileImpl::IsWritable() { SIMMY_STUB("CTGFileImpl::IsWritable(void)", 0x8007CEE8); return {}; }
RET_UNKNOWN CTGFileImpl::Read(void *, int) { SIMMY_STUB("CTGFileImpl::Read(void *, int)", 0x8007CEF0); return {}; }
RET_UNKNOWN CTGFileImpl::Write(void *, int) { SIMMY_STUB("CTGFileImpl::Write(void *, int)", 0x8007CF28); return {}; }
RET_UNKNOWN CTGFileImpl::Seek(int) { SIMMY_STUB("CTGFileImpl::Seek(int)", 0x8007CF30); return {}; }
RET_UNKNOWN CTGFileImpl::SeekPastEnd(int) { SIMMY_STUB("CTGFileImpl::SeekPastEnd(int)", 0x8007CF5C); return {}; }
RET_UNKNOWN CTGFileImpl::Tell() { SIMMY_STUB("CTGFileImpl::Tell(void)", 0x8007CF88); return {}; }
RET_UNKNOWN CTGFileImpl::SetSize(int) { SIMMY_STUB("CTGFileImpl::SetSize(int)", 0x8007CFBC); return {}; }
RET_UNKNOWN CTGFileImpl::Flush() { SIMMY_STUB("CTGFileImpl::Flush(void)", 0x8007CFC4); return {}; }
RET_UNKNOWN CTGFileImpl::FlushCache() { SIMMY_STUB("CTGFileImpl::FlushCache(void)", 0x8007CFCC); return {}; }
bool CTGFileImpl::ReadBytes(unsigned char *, int) { SIMMY_STUB("CTGFileImpl::ReadBytes(unsigned char *, int)", 0x8007CFD4); return {}; }
bool CTGFileImpl::WriteBytes(unsigned char *, int) { SIMMY_STUB("CTGFileImpl::WriteBytes(unsigned char *, int)", 0x8007D018); return {}; }
void CTGFileImpl::ReadByte(unsigned char *) { SIMMY_STUB("CTGFileImpl::ReadByte(unsigned char *)", 0x8007D064); }
void CTGFileImpl::WriteByte(unsigned char) { SIMMY_STUB("CTGFileImpl::WriteByte(unsigned char)", 0x8007D09C); }
void CTGFileImpl::ReadInteger(int *) { SIMMY_STUB("CTGFileImpl::ReadInteger(int *)", 0x8007D0DC); }
void CTGFileImpl::WriteInteger(int) { SIMMY_STUB("CTGFileImpl::WriteInteger(int)", 0x8007D114); }
void CTGFileImpl::ReadFloat(float *) { SIMMY_STUB("CTGFileImpl::ReadFloat(float *)", 0x8007D154); }
void CTGFileImpl::WriteFloat(float) { SIMMY_STUB("CTGFileImpl::WriteFloat(float)", 0x8007D18C); }
RET_UNKNOWN CTGFileImpl::ReadString(char *, int) { SIMMY_STUB("CTGFileImpl::ReadString(char *, int)", 0x8007D1CC); return {}; }
RET_UNKNOWN CTGFileImpl::WriteString(char *) { SIMMY_STUB("CTGFileImpl::WriteString(char *)", 0x8007D28C); return {}; }
CTGFileManager::CTGFileManager() { SIMMY_STUB("CTGFileManager::CTGFileManager(void)", 0x8007D350); }
CTGFileManager::~CTGFileManager() { SIMMY_STUB("CTGFileManager::~CTGFileManager(void)", 0x8007D354); }
RET_UNKNOWN CTGFileManager::Init() { SIMMY_STUB("CTGFileManager::Init(void)", 0x8007D37C); return {}; }
void CTGFileManager::Shutdown() { SIMMY_STUB("CTGFileManager::Shutdown(void)", 0x8007D384); }
RET_UNKNOWN CTGFileManager::OpenFile(char *, bool) { SIMMY_STUB("CTGFileManager::OpenFile(char *, bool)", 0x8007D388); return {}; }
void CTGFileManager::ReleaseFile(CTGFile *) { SIMMY_STUB("CTGFileManager::ReleaseFile(CTGFile *)", 0x8007D484); }
RET_UNKNOWN CTGFileManager::CreateFile(char *) { SIMMY_STUB("CTGFileManager::CreateFile(char *)", 0x8007D4C4); return {}; }
RET_UNKNOWN CTGFileManager::DeleteFile(char *) { SIMMY_STUB("CTGFileManager::DeleteFile(char *)", 0x8007D4CC); return {}; }
RET_UNKNOWN CTGFileManager::MoveFile(char *, char *) { SIMMY_STUB("CTGFileManager::MoveFile(char *, char *)", 0x8007D4D4); return {}; }
RET_UNKNOWN CTGFileManager::CopyFile(char *, char *) { SIMMY_STUB("CTGFileManager::CopyFile(char *, char *)", 0x8007D4DC); return {}; }
bool CTGFileManager::FileExists(char *) { SIMMY_STUB("CTGFileManager::FileExists(char *)", 0x8007D4E4); return {}; }
CTGFile::CTGFile() { SIMMY_STUB("CTGFile::CTGFile(void)", 0x8007D590); }
void CTGFileImpl::GetName() { SIMMY_STUB("CTGFileImpl::GetName(void)", 0x8007D5A4); }
