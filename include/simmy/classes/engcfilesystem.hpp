// @class ENgcFileSystem
#pragma once

struct ENgcFileSystem : public EFileSystem {
  ENgcFileSystem() = default;
  virtual ~ENgcFileSystem();
  virtual int Init(EFile::DeviceType defaultDevice);
};
