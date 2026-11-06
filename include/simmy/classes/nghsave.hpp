// @class NghResFile__SectionID after NghResFile__RegistryID
// @class NghResFile__SaveRecord after NghResFile__RegistryID
// @class NghResFile__RegistryNode after NghResFile__RegistryID
// @class NghLayout after NghResFile__RegistryID
// @class MemoryCardCache after NghResFile__RegistryID
// @class ThumbnailCompresser after NghResFile__RegistryID
// The memory card side of the neighbourhood save (game/nghresfile.cpp): the save file's layout
// (s_nghLayout), the card cache that holds every section between the game and the card, the
// registry of extra sections other systems save into the file, and the thumbnails' compressor.
#pragma once

// A registered section's sub-section ('Uix1'..'Uix4' of 'Uixx').
struct NghResFile__SectionID {
  SIMMY_ENUM_BODY(NghResFile__SectionID)
};

// The bytes of a registered sub-section, saved or loaded (8 bytes on GC).
struct NghResFile__SaveRecord {
  unsigned int m_size = 0;  // +0x00
  void* m_data = nullptr;   // +0x04
};

// A registered section and its callbacks (0x10 bytes).
struct NghResFile__RegistryNode {
  NghResFile__RegistryID m_id;                                          // +0x00
  void (*m_save)(NghResFile__SectionID, NghResFile__SaveRecord&) = nullptr;      // +0x04
  void (*m_load)(NghResFile__SectionID, NghResFile__SaveRecord&) = nullptr;      // +0x08
  void (*m_complete)(NghResFile__SectionID, NghResFile__SaveRecord&) = nullptr;  // +0x0C
};

// The save file as kinds of fixed-size sections, each kind `count` sections of `size` bytes, in file
// order (GC 0x18 bytes). Kinds that belong to a registered section name it and its first sub-section.
struct NghLayout {
  struct Entry {
    int count;               // +0x00
    int size;                // +0x04
    unsigned int offset;     // +0x08 where the kind starts (set at start-up)
    unsigned int total;      // +0x0C count * size (set at start-up)
    unsigned int registryId; // +0x10
    unsigned int sectionId;  // +0x14
  };
  RET_UNKNOWN GetSectionNumberForOffset(unsigned int offset);  // 800D2700
  int GetSubSectionNumberForOffset(unsigned int offset);  // 800D2744
  int IsOffsetARegistrySubSection(unsigned int offset, NghResFile__RegistryID& id, NghResFile__SectionID& section);  // 800D27CC

  int m_version = 0;           // +0x00
  Entry* m_entries = nullptr;  // +0x04 ended by a zero entry
  int m_numKinds = 0;          // +0x08
  int m_numSections = 0;       // +0x0C sub-sections in all
  unsigned int m_totalSize = 0;    // +0x10 the file's size
  unsigned int m_maxSize = 0;      // +0x14 the largest section
};
extern NghLayout s_nghLayout;  // 8048632C (version 8)

// Every section of the save as it is (or will be) on the card, GameCube byte order (0x1C bytes).
struct MemoryCardCache {
  RET_UNKNOWN GetMemoryUsage(NghLayout *);  // 800D29DC
  RET_UNKNOWN GetSubSectionMemoryUsage(NghLayout *, int);  // 800D2A68
  int Init(NghLayout* layout);  // 800D2AB4
  void Destroy();  // 800D2BEC
  int GetSection(NghLayout* layout, char* dst, unsigned int offset, unsigned int size);  // 800D2CC0
  int PutSection(NghLayout* layout, char* src, unsigned int offset, unsigned int size);  // 800D2D9C
  int CacheNghSection(NghLayout* layout, char* data, unsigned int offset, unsigned int size);  // 800D2E7C
  int LoadCacheFromMemoryCard(int card);  // 800D2FBC
  int LoadCacheFromMemoryCard(int card, NghResFile* ngh);  // 800D2FE0
  int LoadConfigFromMemoryCard(int card);  // 800D3258
  int SaveAllToMemoryCard(int card);  // 800D333C
  int GetConfig(char* dst, unsigned int size, int card);  // 800D3720
  int PutConfig(char* src, unsigned int size);  // 800D37C8

  unsigned char* m_config = nullptr;     // +0x00 the options (0x2000 bytes at 0x2000 in the file)
  unsigned char** m_sections = nullptr;  // +0x04 one block per sub-section
  int m_valid = 0;                       // +0x08 holds sections
  int m_configValid = 0;                 // +0x0C holds the options
  int m_initialized = 0;                 // +0x10
  int m_noChecks = 0;                    // +0x14 (set 0 on loading; headers taken as they are when set)
  NghLayout* m_layout = nullptr;         // +0x18
};

// A thumbnail (12-byte header, 32x32 C8 pixels, 256 RGBA8 palette entries) stored with its palette's
// alpha as one bit per entry.
struct ThumbnailCompresser {
  static int Encode(unsigned char* in, int inSize, unsigned char* out, int outSize);  // 800D8F00
  static int Decode(unsigned char* in, int inSize, unsigned char* out, int outSize);  // 800D90D0
};
