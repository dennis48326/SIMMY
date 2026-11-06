// Port of engine/e_rflash.cpp and engine/e_flashman.cpp: Flash movie resources ('FLSH', version 3)
// and the "flashes" resource manager.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>

namespace {
inline unsigned int Swap32(unsigned int v) {
  return v << 24 | (v & 0xff00) << 8 | (v >> 8 & 0xff00) | v >> 24;
}
inline unsigned int ReadBE32(const unsigned char* p) {
  return static_cast<unsigned int>(p[0]) << 24 | static_cast<unsigned int>(p[1]) << 16 |
         static_cast<unsigned int>(p[2]) << 8 | p[3];
}
inline void SwapField32(unsigned char* p) {
  // The .tgq header fields CreateTexture swaps were written little-endian by the PC tools.
  unsigned char t = p[0];
  p[0] = p[3];
  p[3] = t;
  t = p[1];
  p[1] = p[2];
  p[2] = t;
}
inline void SwapField16(unsigned char* p) {
  const unsigned char t = p[0];
  p[0] = p[1];
  p[1] = t;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// ERFlash

// 803133E0
ERFlash::ERFlash() {}

// 80313424
ERFlash::~ERFlash() { Deallocate(); }

// 80313484
void ERFlash::Deallocate() {
  delete m_big;
  m_big = nullptr;
}

// 803134C8
// The data header ('FLSH', version 3) is followed by the movie's BIG archive.
void ERFlash::Load(EFile* file, unsigned int) {
  Deallocate();
  EDataHeader header;
  if (!header.Read(*file, 0x464C5348 /* 'FLSH' */, 3, 3)) return;
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  m_big = new FlashBigFile;
  m_big->LoadFiles(file);
}

// 80313574
// Hands the loaded files over to the caller (the Apt loader) and detaches the resource from its
// manager; the caller then owns the FlashBigFile.
FlashBigFile* ERFlash::GetFlashBigData() {
  Detach();
  FlashBigFile* big = m_big;
  m_big = nullptr;
  return big;
}

// 80313C64 ERFlash::New
// 80313C8C ERFlash::Construct
// 80313CB8 ERFlash::Destruct
// 80313CDC ERFlash::SafeDelete
// 80313D1C ERFlash::GetTypeInfo
// 80313D28 ERFlash::GetTypeName
// 80313D34 ERFlash::GetTypeKey
// 80313D40 ERFlash::GetTypeVersion
// 80313D4C ERFlash::GetTypeInfoStatic
// 80313D58 ERFlash::GetReadVersion
SIMMY_STORABLE_IMPL(ERFlash, "ERFlash", 0, EResource)

// 80313D64
void ERFlash::RegisterType(unsigned short) {}
// 80313DB8
void ERFlash::CreateCopy() const {}

// ---------------------------------------------------------------------------------------------------
// FlashBigFile

// 803135AC
FlashBigFile::FlashBigFile() {}

// 803135C0
// Textures are released through their resource; every other block goes back to the flash manager.
FlashBigFile::~FlashBigFile() {
  if (!m_header) return;
  const int count = BIG_count(m_header);
  for (int i = 0; i < count; ++i) {
    if (!IsTexture(i)) {
      _flashman.Free(m_entries[i]);
    } else if (auto* tex = static_cast<FlashBigTexture*>(m_entries[i])) {
      if (tex->texture) tex->texture->DelRef();
      delete tex;
    }
    m_entries[i] = nullptr;
  }
  _flashman.Free(m_entries);
  m_entries = nullptr;
  _flashman.Free(m_header);
  m_header = nullptr;
}

// 803136C8
bool FlashBigFile::IsTexture(int index) {
  const char* name = BIG_locateentry(m_header, nullptr, &index, nullptr, nullptr);
  const size_t len = std::strlen(name);
  return len >= 4 && BIG_stricmp(const_cast<char*>(name) + len - 4, const_cast<char*>(".tgq")) == 0;  // GC: strcasecmp
}

// 8031373C
// Reads the BIG directory, then each entry into its own block (entries of size -1, the "*.wav"
// placeholder, stay null). Entry offsets are relative to the start of the archive.
void FlashBigFile::LoadFiles(EFile* file) {
  const int base = file->Tell();
  unsigned char head[0x18];
  file->Read(head, sizeof(head));
  const unsigned int headerSize = BIG_sizeofheader(head);
  m_header = static_cast<unsigned char*>(_flashman.Alloc(headerSize, 8));
  std::memcpy(m_header, head, sizeof(head));
  file->Read(m_header + sizeof(head), headerSize - sizeof(head));
  const int count = BIG_count(m_header);
  m_entries = static_cast<void**>(_flashman.Alloc(sizeof(void*) * count, 8));
  for (int i = 0; i < count; ++i) {
    int index = i, offset = 0, size = 0;
    if (!BIG_locateentry(m_header, nullptr, &index, &offset, &size) || size == -1) {
      m_entries[i] = nullptr;
      continue;
    }
    m_entries[i] = _flashman.Alloc(static_cast<unsigned long>(size), 8);
    file->Seek(base + offset, EFile__SeekType::Set);
    file->Read(m_entries[i], static_cast<unsigned int>(size));
    if (IsTexture(i)) m_entries[i] = CreateTexture(i);
  }
}

// 80313948
// The archive's name follows the 0x18-byte BIG header.
char* FlashBigFile::GetFlashName() { return reinterpret_cast<char*>(m_header + 0x18); }

// 80313954
void* FlashBigFile::GetDataSection(char* name) {
  int index = 0, offset = 0, size = 0;
  BIG_locateentry(m_header, name, &index, &offset, &size);
  return m_entries[index];
}

// 803139A0
char* FlashBigFile::GetGeometry(char* name) {
  char* data = static_cast<char*>(GetDataSection(name));
  if (std::strcmp(data, "Apt PCGL geometry file\x1a") != 0) data = nullptr;
  return data;
}

// 803139E8
// Returns the texture with a new reference, and the bitmap's size.
ERTexture* FlashBigFile::GetTexture(char* name, int& width, int& height) {
  auto* tex = static_cast<FlashBigTexture*>(GetDataSection(name));
  tex->texture->AddRef();
  width = tex->width;
  height = tex->height;
  return tex->texture;
}

// 80313A38
// A .tgq is a 'TXFL' texture image (C8 with a 32-bit palette) whose header was written little-endian;
// the fields the texture loader reads are swapped to big-endian first. Two words after the image and
// palette give the bitmap's size in the movie. The raw block is freed once the texture is made.
FlashBigTexture* FlashBigFile::CreateTexture(int index) {
  auto* data = static_cast<unsigned char*>(m_entries[index]);
  SwapField32(data + 0x00);
  SwapField16(data + 0x14);
  SwapField16(data + 0x16);
  const int pixels = (data[0x14] << 8 | data[0x15]) * (data[0x16] << 8 | data[0x17]);
  SwapField16(data + 0x18);
  SwapField16(data + 0x1A);
  SwapField32(data + 0x0C);
  const unsigned int width = Swap32(ReadBE32(data + pixels + 0x424));
  const unsigned int height = Swap32(ReadBE32(data + pixels + 0x428));
  auto* texture = new ERTexture;  // GC: in _textureman's heap, unmanaged
  char name[0x40];
  int entry = index;
  std::snprintf(name, sizeof(name), "%.32s%.16s", GetFlashName(),
                BIG_locateentry(m_header, nullptr, &entry, nullptr, nullptr));
  (void)name;  // GC: formatted for a debug name that is never stored
  texture->Load(data);
  auto* tex = new FlashBigTexture;
  tex->texture = texture;
  tex->width = static_cast<int>(width);
  tex->height = static_cast<int>(height);
  _flashman.Free(data);
  return tex;
}

// ---------------------------------------------------------------------------------------------------
// EFlashManager

// 80324BD4
EAHeap* EFlashManager::GetHeap() { return VMHeap(); }

// 80324BF4
EResource* EFlashManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* flash = new ERFlash;  // GC: in this manager's heap
  flash->m_id = id;
  flash->Load(file, size);
  return flash;
}

// 80324CC0
EResource* EFlashManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 80324CE0
EResource* EFlashManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }

// 80324D00
int EFlashManager::CanCache(EResource*) { return 1; }
