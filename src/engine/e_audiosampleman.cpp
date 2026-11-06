// Port of engine/e_audiosampleman.cpp.
#include "simmy/types_gen.hpp"

// 8036C33C
EAHeap* EAudioSampleManager::GetHeap() { return VMHeap(); }
