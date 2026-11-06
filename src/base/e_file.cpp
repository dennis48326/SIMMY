// Port of base/e_file.cpp: the abstract file and path helpers.
// Still stubbed: operator>>(EFile&, EString&) (needs EString).
#include "simmy/types_gen.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

static bool m_bSimulateError = false;

// 802C6048
EFile::EFile() = default;

// 802C608C
EFile::~EFile() {
  MainHeap()->Free(m_path);
  MainHeap()->Free(m_name);
  m_path = m_name = nullptr;
}

// 802C60FC
void EFile::SetErrorSimulation(bool simulate) { m_bSimulateError = simulate; }

// 802C6104
// Splits "C:dir/sub/name.ext" into drive ("C:"), dir ("dir/sub/"), name ("name") and ext (".ext").
// Each output may be null; components are clipped to 255 characters.
void SplitPath(char* path, char* drive, char* dir, char* name, char* ext) {
  auto copy = [](char* out, const char* from, size_t n) {
    if (n > 0xff) n = 0xff;
    std::strncpy(out, from, n);
    out[n] = '\0';
  };
  if (std::strlen(path) != 0 && path[1] == ':') {
    if (drive) copy(drive, path, 2);
    path += 2;
  } else if (drive) {
    *drive = '\0';
  }
  const char* fileStart = nullptr;
  const char* dot = nullptr;
  const char* end = path;
  for (; *end; ++end) {
    if (*end == '/' || *end == '\\') fileStart = end + 1;
    else if (*end == '.') dot = end;
  }
  if (!fileStart) {
    fileStart = path;
    if (dir) *dir = '\0';
  } else if (dir) {
    copy(dir, path, static_cast<size_t>(fileStart - path));
  }
  if (!dot || dot < fileStart) {
    if (name) copy(name, fileStart, static_cast<size_t>(end - fileStart));
    if (ext) *ext = '\0';
  } else {
    if (name) copy(name, fileStart, static_cast<size_t>(dot - fileStart));
    if (ext) copy(ext, dot, static_cast<size_t>(end - dot));
  }
}

// 802C62E8
char* DupString(char* s) {
  if (!s) return nullptr;
  char* out = static_cast<char*>(MainHeap()->Malloc(static_cast<unsigned int>(std::strlen(s) + 1), 0));
  if (out) std::strcpy(out, s);
  return out;
}

// 802C6354
void EFile::SetName(char* fullName) {
  char drive[8], dir[256], name[256], ext[260];
  SplitPath(fullName, drive, dir, name, ext);
  const char* e = (ext[0] == '.') ? ext + 1 : ext;
  std::strncpy(m_drive, drive, 3);
  m_drive[3] = '\0';
  std::strncpy(m_ext, e, 3);
  m_ext[3] = '\0';
  MainHeap()->Free(m_path);
  m_path = DupString(dir);
  MainHeap()->Free(m_name);
  m_name = DupString(name);
}

// 802C6428
int EFile::Fprintf(const char* format, ...) {
  char buf[1024];
  va_list args;
  va_start(args, format);
  const int n = std::vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if (n > 0) Write(buf, static_cast<unsigned int>(n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1));
  return n;
}

// 802C6904
int EFile::Allocate(unsigned int&) { return 0; }
// 802C690C
int EFile::GetIOMode() const { return m_ioMode; }
// 802C6914
int EFile::GetAccessMode() const { return m_accessMode; }
// 802C691C
int EFile::GetDeviceType() const { return m_deviceType; }
// 802C6924
const char* EFile::GetDrive() const { return m_drive; }
// 802C692C
const char* EFile::GetPath() const { return m_path; }
// 802C6934
const char* EFile::GetName() const { return m_name; }
// 802C693C
const char* EFile::GetExt() const { return m_ext; }
// 802C6944
int EFile::GetLength() const { return m_length; }
// 802C694C
void EFile::SetDevice(EFile::DeviceType device) { m_deviceType = device; }
// 802C6954
void EFile::SetAccess(EFile::AccessMode access) { m_accessMode = access; }
// 802C695C
void EFile::SetMode(EFile::IOMode mode) { m_ioMode = mode; }

// ---- Object graphs -------------------------------------------------------------------------------
// A stored pointer is a 1-based index into the current structure's object table (0 = null). The first
// pointer read outside a structure starts one: u32 count, then groups of {u32 type key, u16 version,
// u32 objects of that type} until `count` objects exist, then each object's Load followed by one pad
// byte, then u32 = the index of the root object.

// 802C64F8
EStorable* EFile::ReadStructure(unsigned int count) {
  m_storableDepth = 1;
  auto** objects = new EStorable*[count]();
  m_storables = reinterpret_cast<void**>(objects);
  for (unsigned int made = 0; made < count;) {
    const unsigned int key = simmy::read_u32(*this);
    ETypeInfo* type = ETypeInfo::Find(key);
    const unsigned short version = simmy::read_u16(*this);
    unsigned int n = simmy::read_u32(*this);
    if (!type) {
      std::fprintf(stderr, "[storable] unknown type key %08X (%u objects) - class not ported\n", key, n);
      m_storableDepth = 0;
      m_storables = nullptr;
      delete[] objects;
      return nullptr;
    }
    type->m_readVersion = version;
    for (; n > 0 && made < count; --n) objects[made++] = type->m_new();
  }
  for (unsigned int i = 0; i < count; ++i) {
    objects[i]->Load(*this);
    simmy::read_u8(*this);
  }
  const unsigned int root = simmy::read_u32(*this);
  EStorable* result = objects[root];
  delete[] objects;
  m_storables = nullptr;
  m_storableDepth = 0;
  return result;
}

// 802C6760
// An object stored in place: one u16 version per class from its own up to EStorable, then its data.
EFile& operator>>(EFile& file, EStorable& obj) {
  for (ETypeInfo* t = obj.GetTypeInfo(); t; t = t->m_parent) t->m_readVersion = simmy::read_u16(file);
  obj.Load(file);
  return file;
}

// 802C67FC
EFile& operator>>(EFile& file, EStorable*& obj) {
  const unsigned int index = simmy::read_u32(file);
  if (!index) obj = nullptr;
  else if (!file.m_storableDepth) obj = file.ReadStructure(index);
  else obj = reinterpret_cast<EStorable**>(file.m_storables)[index - 1];
  return file;
}
