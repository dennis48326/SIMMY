// GENERATED stubs for unported functions of engine/e_msgqueue.cpp. Do not edit.
#include "simmy/types_gen.hpp"

EMsgQueue::EMsgQueue() { SIMMY_STUB("EMsgQueue::EMsgQueue(void)", 0x8035C4A4); }
EMsgQueue::EMsgQueue(int) { SIMMY_STUB("EMsgQueue::EMsgQueue(int)", 0x8035C4F0); }
EMsgQueue::~EMsgQueue() { SIMMY_STUB("EMsgQueue::~EMsgQueue(void)", 0x8035C548); }
RET_UNKNOWN EMsgQueue::Create(int) { SIMMY_STUB("EMsgQueue::Create(int)", 0x8035C5AC); return {}; }
void EMsgQueue::Destroy() { SIMMY_STUB("EMsgQueue::Destroy(void)", 0x8035C698); }
RET_UNKNOWN EMsgQueue::Send(unsigned int, bool) { SIMMY_STUB("EMsgQueue::Send(unsigned int, bool)", 0x8035C6F0); return {}; }
RET_UNKNOWN EMsgQueue::Receive(unsigned int *, bool) { SIMMY_STUB("EMsgQueue::Receive(unsigned int *, bool)", 0x8035C7B0); return {}; }
bool EMsgQueue::iSend(unsigned int) { SIMMY_STUB("EMsgQueue::iSend(unsigned int)", 0x8035C850); return {}; }
RET_UNKNOWN EMsgQueue::iReceive(unsigned int *) { SIMMY_STUB("EMsgQueue::iReceive(unsigned int *)", 0x8035C8C8); return {}; }
RET_UNKNOWN EMsgQueue::GetCount() { SIMMY_STUB("EMsgQueue::GetCount(void)", 0x8035C950); return {}; }
void EMsgQueue::GetStats(int &, int &, int &, int &) { SIMMY_STUB("EMsgQueue::GetStats(int &, int &, int &, int &)", 0x8035C974); }
RET_UNKNOWN EMsgQueue::Empty() { SIMMY_STUB("EMsgQueue::Empty(void)", 0x8035C9CC); return {}; }
