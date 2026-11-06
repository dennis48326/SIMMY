// @class ENgcFile
// Platform file. GameCube: a DVD file read through a 4 KB sector buffer. PC: a read-only game file
// (simmy::DataFile: a file on the disc image, or in the --data folder; see simmy/platform.hpp).
#pragma once

namespace simmy {
struct DataFile;
}

struct ENgcFile : public EFile {
  ENgcFile() = default;
  virtual ~ENgcFile();
  static EFile* Creator(EFile* existing, char* name, char* mode, EFile::DeviceType device,
                        EFile::AccessMode access, unsigned int flags);
  virtual void Destroy();
  virtual int Read(void* dst, unsigned int size);
  RET_UNKNOWN BufferReadRequest(unsigned char *, int);  // 802E1A8C (GameCube sector buffer; unused on PC)
  virtual int Write(void* src, unsigned int size);
  virtual int Seek(int offset, EFile::SeekType type);
  virtual int Tell() const;
  virtual int Flush();
  virtual int GetLastError() const;
  virtual void* GetSystemHandle() const;

  simmy::DataFile* m_file = nullptr;  // replaces the DVDFileInfo at +0x2C and the sector buffer at +0x6C
  int m_error = 0;
};
