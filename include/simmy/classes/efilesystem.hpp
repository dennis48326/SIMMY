// @class EFileSystem after EFileSystem__FileCreator EGlobalManagerClient
// File system front end (base/e_filesystem.cpp). Platform files register creators with it.
#pragma once
#include <vector>

struct EFileSystem : public EGlobalManagerClient {
  using FileCreator = ::EFileSystem__FileCreator;
  EFileSystem();
  virtual ~EFileSystem();
  virtual void ManagedShutdown();
  virtual int Access(char* name, char* mode, EFile::DeviceType device, EFile::AccessMode access);
  virtual int Create(EFile*& file, char* name, char* mode, EFile::DeviceType device, EFile::AccessMode access,
                     unsigned int flags);
  virtual int Init(EFile::DeviceType defaultDevice);
  virtual int InitHDD(bool format, char* path);
  virtual int InitHDDFileSystem();
  virtual int HDDIsUnformatted();
  FileCreator::CreatorFn FindCreator(EFile::DeviceType device, EFile::IOMode mode, EFile::AccessMode access,
                                     char* ext) const;
  void Destroy(EFile*& file);
  bool HDDInitialized();
  int HDDStatus();
  bool HDDTmpInitialized();
  int HDDTmpStatus();
  bool HDDSaveInitialized();
  int HDDSaveStatus();
  int GetDefaultType() const;
  int RegisterFileCreator(EFile::DeviceType device, EFile::IOMode mode, EFile::AccessMode access, char* ext,
                          FileCreator::CreatorFn fn);
  int ParseMode(char* mode, EFile::IOMode& out);

  std::vector<FileCreator> m_creators;  // +0x04 (TArray)
  int m_defaultDevice = 0;              // +0x10
  int m_initialized = 0;                // +0x14
  int m_hddStatus = 4;                  // +0x18 (0xB = initialised)
  int m_hddTmpStatus = 4;               // +0x1C
  int m_hddSaveStatus = 4;              // +0x20
};
