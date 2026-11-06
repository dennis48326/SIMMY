// Port of base/e_ngcfilesystem.cpp: the platform file system and the C-style ef* helpers.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"

#include <cstdio>

ENgcFileSystem _eorFileSys;  // 8049D684, the engine's file system

// 802D73C0  ENgcFileSystem::ENgcFileSystem (defaulted in the header)

// 802D73FC
ENgcFileSystem::~ENgcFileSystem() {
  if (!EGlobalManager::m_shutdownComplete) EGlobalManager::Shutdown();
}

// 802D7450
int ENgcFileSystem::Init(EFile::DeviceType defaultDevice) {
  if (defaultDevice == static_cast<int>(0x80000000u)) defaultDevice = EFile__DeviceType::Disc;
  const int ok = EFileSystem::Init(defaultDevice);
  // Original: ENgcFile for disc files (any device) and ENgcSNFile for the SN host-PC device.
  // On PC both are game files (disc image or --data folder), so ENgcFile serves both.
  const int any = static_cast<int>(0x80000000u);
  if (ok) RegisterFileCreator(any, any, any, nullptr, ENgcFile::Creator);
  RegisterFileCreator(any, EFile__DeviceType::SNHost, any, nullptr, ENgcFile::Creator);
  if (!simmy::data_exists("data/index.ind"))
    std::fprintf(stderr, "[file] WARNING: game data not found (no DATA/index.ind in the %s).\n",
                 simmy::data_source_description().c_str());
  return ok;
}

// 802D74E8
extern "C" EFile* efopen(char* name, char* mode) {
  EFile* f = nullptr;
  _eorFileSys.Create(f, name, mode, static_cast<int>(0x80000000u), static_cast<int>(0x80000000u), 0);
  return f;
}

// 802D7534
extern "C" int efclose(EFile* f) {
  _eorFileSys.Destroy(f);
  return 0;
}

// 802D7568
extern "C" int efread(void* dst, int size, int count, EFile* f) {
  return f->Read(dst, static_cast<unsigned int>(size * count));
}

// 802D75A4
extern "C" int efwrite(void* src, int size, int count, EFile* f) {
  return f->Write(src, static_cast<unsigned int>(size * count));
}

// 802D75E0
extern "C" int efseek(EFile* f, int offset, int whence) {
  static const int lutSeekType[3] = {EFile__SeekType::Set, EFile__SeekType::Cur, EFile__SeekType::End};
  f->Seek(offset, lutSeekType[whence]);
  return f->Flush();
}

// 802D7648
extern "C" int eftell(EFile* f) { return f->Tell(); }

// 802D767C
extern "C" int efflush(EFile* f) { return f->Flush(); }

// 802D76B0
extern "C" char* efgets(char* buf, int size, EFile* f) {
  if (size < 1) return nullptr;
  char* out = buf;
  while (--size > 0) {
    char c;
    if (f->Read(&c, 1) == 0) {
      if (out == buf) return nullptr;
      break;
    }
    *out++ = c;
    if (c == '\n') break;
  }
  *out = '\0';
  return buf;
}
