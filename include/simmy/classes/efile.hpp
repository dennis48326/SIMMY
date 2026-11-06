// @class EFile after EFile__DeviceType
// Abstract file (base/e_file.cpp). Layout from the constructor; the vtable pointer was at +0x28.
#pragma once

struct EFile {
  using AccessMode = ::EFile__AccessMode;
  using DeviceType = ::EFile__DeviceType;
  using IOMode = ::EFile__IOMode;
  using SeekType = ::EFile__SeekType;

  EFile();
  virtual ~EFile();
  virtual void Destroy() = 0;                              // closes the file and deletes it
  virtual int Read(void* dst, unsigned int size) = 0;      // returns bytes read
  virtual int Write(void* src, unsigned int size) = 0;     // returns bytes written
  virtual int Seek(int offset, EFile::SeekType type) = 0;  // returns the new position
  virtual int Tell() const = 0;
  virtual int Flush() = 0;
  virtual int GetLastError() const = 0;
  virtual void* GetSystemHandle() const = 0;
  virtual int Allocate(unsigned int& size);

  static void SetErrorSimulation(bool simulate);
  void SetName(char* fullName);
  int Fprintf(const char* format, ...);
  EStorable* ReadStructure(unsigned int count);
  int GetIOMode() const;
  int GetAccessMode() const;
  int GetDeviceType() const;
  const char* GetDrive() const;
  const char* GetPath() const;
  const char* GetName() const;
  const char* GetExt() const;
  int GetLength() const;
  void SetDevice(EFile::DeviceType device);
  void SetAccess(EFile::AccessMode access);
  void SetMode(EFile::IOMode mode);

  int m_storableDepth = 0;            // +0x00, used by ReadStructure
  void** m_storables = nullptr;       // +0x04
  int m_ioMode = static_cast<int>(0x80000000u);      // +0x08
  int m_accessMode = static_cast<int>(0x80000000u);  // +0x0C
  int m_deviceType = static_cast<int>(0x80000000u);  // +0x10
  int m_length = 0;                   // +0x14
  char m_drive[4] = {};               // +0x18, e.g. "C:"
  char* m_path = nullptr;             // +0x1C, directory part (heap)
  char* m_name = nullptr;             // +0x20, base name without extension (heap)
  char m_ext[4] = {};                 // +0x24, extension without the dot
};
