// GENERATED stubs for unported functions of engine/e_ngcaram.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void ENgcAram::Open() { SIMMY_STUB("ENgcAram::Open(void)", 0x8036C35C); }
void ENgcAram::Close() { SIMMY_STUB("ENgcAram::Close(void)", 0x8036C40C); }
RET_UNKNOWN ENgcAram::Allocate(unsigned int) { SIMMY_STUB("ENgcAram::Allocate(unsigned int)", 0x8036C424); return {}; }
RET_UNKNOWN ENgcAram::AvailableMemory() { SIMMY_STUB("ENgcAram::AvailableMemory(void)", 0x8036C468); return {}; }
void ENgcAram::SequentialLoadToARAM(void *, unsigned int, unsigned int) { SIMMY_STUB("ENgcAram::SequentialLoadToARAM(void *, unsigned int, unsigned int)", 0x8036C47C); }
void AramRequestCallback(unsigned long) { SIMMY_STUB("AramRequestCallback(unsigned long)", 0x8036C510); }
void ENgcAram::AsyncLoadToARAM(void *, unsigned int, unsigned int, void (*)(unsigned int), unsigned int) { SIMMY_STUB("ENgcAram::AsyncLoadToARAM(void *, unsigned int, unsigned int, void (*)(unsigned int), unsigned int)", 0x8036C51C); }
RET_UNKNOWN AsyncAramRequestCallback(unsigned long) { SIMMY_STUB("AsyncAramRequestCallback(unsigned long)", 0x8036C614); return {}; }
void ENgcAram::SequentialLoadFromARAM(void *, unsigned int, unsigned int) { SIMMY_STUB("ENgcAram::SequentialLoadFromARAM(void *, unsigned int, unsigned int)", 0x8036C6B0); }
void AramRequestCallback2(unsigned long) { SIMMY_STUB("AramRequestCallback2(unsigned long)", 0x8036C744); }
void ENgcAram::AsyncLoadFromARAM(void *, unsigned int, unsigned int, void (*)(unsigned int), unsigned int) { SIMMY_STUB("ENgcAram::AsyncLoadFromARAM(void *, unsigned int, unsigned int, void (*)(unsigned int), unsigned int)", 0x8036C750); }
