// Port of base/e_ngcfile.cpp: the platform file, rewritten for PC on simmy::DataFile (a file of the
// disc image or of the --data folder, see simmy/platform.hpp).
// The GameCube version read the disc in 0x1000-byte chunks through DVDReadPrio into a 0x800-aligned
// buffer; DataFile reads ahead for us. Game files are read-only: opening one for writing fails, as
// DVDOpen offered no writing. (The SN host-PC device of EA's dev kits is not used by the game.)
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"

#include <cstdio>
#include <string>

// 802E185C  ENgcFile::ENgcFile (defaulted in the header)

// 802E18B4
ENgcFile::~ENgcFile() {
  if (m_file) simmy::data_close(m_file);
}

// 802E1918
EFile* ENgcFile::Creator(EFile*, char* name, char* mode, EFile::DeviceType device, EFile::AccessMode access,
                         unsigned int) {
  const bool writing = mode && (std::string(mode).find_first_of("wa+") != std::string::npos);
  if (writing) {
    std::fprintf(stderr, "[file] %s: game files are read-only, not opened for writing\n", name ? name : "?");
    return nullptr;
  }
  simmy::DataFile* data = simmy::data_open(name);
  if (!data) return nullptr;  // DVDOpen failed
  auto* f = new ENgcFile();
  f->m_file = data;
  f->m_accessMode = access;
  f->m_deviceType = device;
  f->m_ioMode = static_cast<int>(0x80000000u);
  f->m_length = static_cast<int>(simmy::data_size(data));
  f->SetName(name);
  return f;
}

// 802E19DC
void ENgcFile::Destroy() { delete this; }

// 802E1A30
int ENgcFile::Read(void* dst, unsigned int size) {
  const size_t n = simmy::data_read(m_file, dst, size);
  if (n < size && simmy::data_error(m_file)) m_error = 1;
  return static_cast<int>(n);
}

// 802E1B78
int ENgcFile::Write(void*, unsigned int) { return 0; }

// 802E1B80
int ENgcFile::Seek(int offset, EFile::SeekType type) {
  // Note the original treats End as `length - offset` (offset counts back from the end).
  long long target = offset;
  if (type == EFile__SeekType::Cur) target = static_cast<long long>(simmy::data_tell(m_file)) + offset;
  else if (type != EFile__SeekType::Set) target = static_cast<long long>(m_length) - offset;
  if (target >= 0) simmy::data_seek(m_file, static_cast<unsigned long long>(target));  // like fseek: no move before 0
  return static_cast<int>(simmy::data_tell(m_file));
}

// 802E1BDC
int ENgcFile::Tell() const { return static_cast<int>(simmy::data_tell(m_file)); }

// 802E1BE4
int ENgcFile::Flush() { return 1; }

// 802E1BEC
int ENgcFile::GetLastError() const { return m_error; }

// 802E1BF4
void* ENgcFile::GetSystemHandle() const { return m_file; }
