// @class EFileSystem__FileCreator after EFile__DeviceType
// One registered file factory, scored against a request by EFileSystem::FindCreator.
#pragma once

struct EFileSystem__FileCreator {
  using CreatorFn = EFile* (*)(EFile*, char*, char*, EFile__DeviceType, EFile__AccessMode, unsigned int);
  EFileSystem__FileCreator();
  EFileSystem__FileCreator(CreatorFn fn, EFile__DeviceType device, EFile__IOMode mode, EFile__AccessMode access,
                           char* ext);
  bool operator==(const EFileSystem__FileCreator& other) const;
  int TallyBits(int mine, int wanted, int bit) const;
  int GetScore(EFile__DeviceType device, EFile__IOMode mode, EFile__AccessMode access, char* ext) const;
  CreatorFn GetCreatorCB() const;

  CreatorFn m_fn = nullptr;  // +0x00
  int m_device = 0;          // +0x04
  int m_mode = 0;            // +0x08
  int m_access = 0;          // +0x0C
  char m_ext[4] = {};        // +0x10, without the dot
};
