// GENERATED stubs for unported functions of engine/hddthread.cpp. Do not edit.
#include "simmy/types_gen.hpp"

ArcCopier::ArcCopier() { SIMMY_STUB("ArcCopier::ArcCopier(void)", 0x80321430); }
ArcCopier::~ArcCopier() { SIMMY_STUB("ArcCopier::~ArcCopier(void)", 0x8032144C); }
void IdleCheck() { SIMMY_STUB("IdleCheck(void)", 0x803214BC); }
bool ArcCopier::Copy(char *, char *, unsigned int &, int) { SIMMY_STUB("ArcCopier::Copy(char *, char *, unsigned int &, int)", 0x8032153C); return {}; }
void ArcCopier::TerminateCopy() { SIMMY_STUB("ArcCopier::TerminateCopy(void)", 0x80321704); }
void ArcCopier::ResumeCopy() { SIMMY_STUB("ArcCopier::ResumeCopy(void)", 0x80321710); }
RET_UNKNOWN ArcCopier::IsTerminatingCopy() { SIMMY_STUB("ArcCopier::IsTerminatingCopy(void)", 0x8032171C); return {}; }
ArcFileInfo::ArcFileInfo() { SIMMY_STUB("ArcFileInfo::ArcFileInfo(void)", 0x80321724); }
void ArcFileInfo::Initialize(char *, char *) { SIMMY_STUB("ArcFileInfo::Initialize(char *, char *)", 0x80321744); }
void ArcFileInfo::OnValidated() { SIMMY_STUB("ArcFileInfo::OnValidated(void)", 0x80321760); }
void ArcFileInfo::Validate() { SIMMY_STUB("ArcFileInfo::Validate(void)", 0x803217D8); }
void ArcFileInfo::Reset() { SIMMY_STUB("ArcFileInfo::Reset(void)", 0x80321A78); }
RET_UNKNOWN ArcFileInfo::CopyArcFile() { SIMMY_STUB("ArcFileInfo::CopyArcFile(void)", 0x80321B60); return {}; }
RET_UNKNOWN ArcFileInfo::IsUpToDate() { SIMMY_STUB("ArcFileInfo::IsUpToDate(void)", 0x80321C70); return {}; }
RET_UNKNOWN ArcFileInfo::IsOpen() { SIMMY_STUB("ArcFileInfo::IsOpen(void)", 0x80321C98); return {}; }
RET_UNKNOWN ArcFileInfo::IsUsingHDD() { SIMMY_STUB("ArcFileInfo::IsUsingHDD(void)", 0x80321CFC); return {}; }
bool ArcFileInfo::CompareName(char *) { SIMMY_STUB("ArcFileInfo::CompareName(char *)", 0x80321D60); return {}; }
RET_UNKNOWN ArcFileInfo::GetName() { SIMMY_STUB("ArcFileInfo::GetName(void)", 0x80321DA4); return {}; }
RET_UNKNOWN ArcFileInfo::GetSourceLen() { SIMMY_STUB("ArcFileInfo::GetSourceLen(void)", 0x80321DAC); return {}; }
RET_UNKNOWN ArcFileInfo::GetCopiedSize() { SIMMY_STUB("ArcFileInfo::GetCopiedSize(void)", 0x80321DB4); return {}; }
HDDThread::HDDThread() { SIMMY_STUB("HDDThread::HDDThread(void)", 0x80321DBC); }
void HDDThread::InitAndRun() { SIMMY_STUB("HDDThread::InitAndRun(void)", 0x80321E70); }
RET_UNKNOWN HDDThread::IsDoingCommand() { SIMMY_STUB("HDDThread::IsDoingCommand(void)", 0x80321E74); return {}; }
void HDDThread::DoDelete() { SIMMY_STUB("HDDThread::DoDelete(void)", 0x80321EE0); }
void HDDThread::DoCopy() { SIMMY_STUB("HDDThread::DoCopy(void)", 0x80321F14); }
void HDDThread::TerminateCopy() { SIMMY_STUB("HDDThread::TerminateCopy(void)", 0x80321F20); }
void HDDThread::DoFormat() { SIMMY_STUB("HDDThread::DoFormat(void)", 0x80321F54); }
void HDDThread::DoSpeedTest() { SIMMY_STUB("HDDThread::DoSpeedTest(void)", 0x80321F8C); }
void HDDThread::SpeedTest() { SIMMY_STUB("HDDThread::SpeedTest(void)", 0x80321FC0); }
void HDDThread::Main() { SIMMY_STUB("HDDThread::Main(void)", 0x80321FC4); }
RET_UNKNOWN HDDThread::GetArcIndex(char *) { SIMMY_STUB("HDDThread::GetArcIndex(char *)", 0x80321FC8); return {}; }
void HDDThread::ArcIsCurrent(char *) { SIMMY_STUB("HDDThread::ArcIsCurrent(char *)", 0x80322028); }
RET_UNKNOWN HDDThread::ArcIsCurrent(int) { SIMMY_STUB("HDDThread::ArcIsCurrent(int)", 0x80322060); return {}; }
RET_UNKNOWN HDDThread::ArcIsUsingHDD(int) { SIMMY_STUB("HDDThread::ArcIsUsingHDD(int)", 0x8032209C); return {}; }
RET_UNKNOWN HDDThread::ArcIsOpen(int) { SIMMY_STUB("HDDThread::ArcIsOpen(int)", 0x803220D8); return {}; }
RET_UNKNOWN HDDThread::GetArcName(int) { SIMMY_STUB("HDDThread::GetArcName(int)", 0x80322114); return {}; }
RET_UNKNOWN HDDThread::GetArcSourceLen(int) { SIMMY_STUB("HDDThread::GetArcSourceLen(int)", 0x80322158); return {}; }
RET_UNKNOWN HDDThread::GetArcCopiedSize(int) { SIMMY_STUB("HDDThread::GetArcCopiedSize(int)", 0x80322194); return {}; }
void HDDThread::SetPriorityHigh() { SIMMY_STUB("HDDThread::SetPriorityHigh(void)", 0x803221D0); }
void HDDThread::SetPriorityLow() { SIMMY_STUB("HDDThread::SetPriorityLow(void)", 0x80322228); }
void HDDThread::StopAll() { SIMMY_STUB("HDDThread::StopAll(void)", 0x80322280); }
