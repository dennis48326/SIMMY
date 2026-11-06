// @class FlashBigFile after ERTexture
// @class ERFlash after EResource FlashBigFile
// @class EFlashManager after EResourceManager ERFlash
// Flash movies (engine/e_rflash.cpp, e_flashman.cpp). A 'FLSH' resource holds a BIG archive ("BIGF")
// with the Apt files of one movie: <name>.apt (characters, frames, bytecode), <name>.const (the
// constant table), <name>.geo (the pre-triangulated shapes, "Apt PCGL geometry file") and the bitmaps
// as NNN.tgq textures. FlashBigFile loads every entry into its own block; textures become ERTextures.
#pragma once

// A bitmap entry once loaded (FlashBigFile::CreateTexture, 12 bytes on GC).
struct FlashBigTexture {
  ERTexture* texture = nullptr;  // +0x00
  int width = 0;                 // +0x04 the bitmap's size in the movie (the texture may be padded)
  int height = 0;                // +0x08
};

struct FlashBigFile {
  FlashBigFile();  // 803135AC
  ~FlashBigFile();  // 803135C0
  bool IsTexture(int index);  // 803136C8
  void LoadFiles(EFile* file);  // 8031373C
  char* GetFlashName();  // 80313948
  void* GetDataSection(char* name);  // 80313954
  char* GetGeometry(char* name);  // 803139A0
  ERTexture* GetTexture(char* name, int& width, int& height);  // 803139E8
  FlashBigTexture* CreateTexture(int index);  // 80313A38

  unsigned char* m_header = nullptr;  // +0x00 the BIG header (directory)
  void** m_entries = nullptr;         // +0x04 one block per entry (a FlashBigTexture for .tgq), or null
};

struct ERFlash : public EResource {
  ERFlash();  // 803133E0
  SIMMY_STORABLE_DECL(ERFlash)
  ~ERFlash() override;  // 80313424
  void Deallocate();  // 80313484
  void Load(EFile* file, unsigned int size);  // 803134C8
  FlashBigFile* GetFlashBigData();  // 80313574
  static void RegisterType(unsigned short);  // 80313D64
  void CreateCopy() const;  // 80313DB8

  FlashBigFile* m_big = nullptr;  // +0x14
};

struct EFlashManager : public EResourceManager {
  EAHeap* GetHeap() override;  // 80324BD4
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;  // 80324BF4
  EResource* AddRef(unsigned int id, EFile* file, int pos);  // 80324CC0
  EResource* AddRef(char* name, EFile* file, int pos);  // 80324CE0
  int CanCache(EResource* res) override;  // 80324D00
};
// The BIG_* directory helpers (engine/locatbig.cpp) are declared through overrides.tsv.
