// @class EXPORT_DIRECTORY
// @class ERQuickdata after EResource EXPORT_DIRECTORY
// A quickdata resource (engine/e_rquickdata.cpp): an "XImage" of named tables of fixed-size rows,
// generated offline from the game's data definitions. Used for every UI string and many game tables.
//
// File layout (all big-endian):
//   EXPORT_DIRECTORY image header; its dataOffset is also the end of the language directories
//   EXPORT_DIRECTORY languages[(dataOffset - 0x18) / 0x18]   one per language, same layout
//   ... image bytes, fixup streams, language blobs (each found through its directory)
// The image starts with an ImageHeader (version 1, export name such as "urbzui", table count,
// revision) followed by the sorted table directory (TableEntry, 0x14 bytes each). On the GameCube,
// LoadXImage read the image, put the selected language's blob right after it (4-aligned) and
// applyFixups turned every 32-bit offset listed in the fixup streams into an absolute pointer.
//
// PC representation (pointers are 8 bytes, so they cannot be patched into 32-bit words):
//   * The buffer keeps the GameCube layout: image, then the language blob at align4(image size).
//     Every non-pointer word stays big-endian; read fields through the accessors below.
//   * Every pointer word named by the fixup streams holds a big-endian *self-relative* int32 offset
//     (target - address of the word). 0 still means null. So a pointer field can be resolved from
//     the field's own address alone: no image base and no owning ERQuickdata are needed.
//   * A language blob is an array of string pointers ("slots") followed by the UTF-16 strings
//     (checked against every quickdata on the disc). The strings are swapped to host-endian UTF-16
//     at load time (`const unsigned short*`, what ERFont's unsigned short* overloads take). Each slot
//     also gets a native pointer cell (`const unsigned short*`) after the blob, and pointers to a
//     slot elsewhere in the image are redirected to its cell. So a field that the original read as
//     `wchar_t** handle` (e.g. NewUIStrings rows, read by EGlobal::GetNewUIString) resolves to a real
//     `const unsigned short* const*`, and `*handle` is the string, as on the GameCube.
//
// Accessors (static; `p` is any address inside an image, usually a row; `off` is the byte offset the
// original code used, as in *(T*)(row + off)):
//   U32/S32/U16/S16/U8/F32(p, off)  host-endian value of a scalar field
//   Ptr(p, off)                     resolved pointer field (nullptr when null)
//   String(p, off)                  8-bit string field (char*), e.g. names
//   LocSlot(p, off)                 localized string handle (wchar_t** on GC): `*LocSlot(..)` is the text
//   LocString(p, off)               the localized text itself (host-endian UTF-16), or nullptr
// Table views (the `table` returned by getTable/findRow): TableName, TableRows, TableRowNames,
// TableRowName(i), TableRowCount, TableRowSize, TableRow(i).
#pragma once

// The 0x18-byte header of an XImage file and of each of its language blobs.
struct EXPORT_DIRECTORY {
  unsigned int m_size = 0;         // +0x00, 0x18
  unsigned int m_dataOffset = 0;   // +0x04, from the start of the file
  unsigned int m_dataSize = 0;     // +0x08
  unsigned int m_fixupOffset = 0;  // +0x0C, from the start of the file
  unsigned int m_fixupSize = 0;    // +0x10, bytes
  unsigned int m_baseAddress = 0;  // +0x14, address the offsets were generated for (0)
};

struct ERQuickdata : public EResource {
  // Image layout offsets (GameCube, big-endian).
  static constexpr unsigned int kImageTableCount = 0x08;  // u32
  static constexpr unsigned int kImageRevision = 0x0C;    // u32
  static constexpr unsigned int kImageTables = 0x18;      // TableEntry[tableCount]
  static constexpr unsigned int kTableEntrySize = 0x14;
  // TableEntry fields
  static constexpr unsigned int kTableName = 0x00;      // char*
  static constexpr unsigned int kTableRows = 0x04;      // row data
  static constexpr unsigned int kTableRowNames = 0x08;  // char*[rowCount], sorted; null if unnamed
  static constexpr unsigned int kTableRowCount = 0x0C;  // u32
  static constexpr unsigned int kTableRowSize = 0x10;   // u32, 0 is treated as 1

  ERQuickdata();
  ~ERQuickdata() override;
  static void* operator new(std::size_t size);
  static void operator delete(void* p);

  void reset();
  static void UnlockImage();
  static void LockImage();
  void Reload(EFile* file);
  void Load(EFile* file, int language);
  using EResource::Load;

  int getTableIndex(int lo, int hi, const char* name) const;
  const void* getTable(const char* name) const;
  static int getRowIndex(int lo, int hi, const char* name, const void* rowNames);
  const void* getRow(const void* table, const char* name) const;
  const unsigned char* getStartAddr(int tableIndex) const;
  int findTableIndex(int lo, int hi, const void* addr) const;
  const void* findRow(const void* addr, unsigned int* rowIndex) const;
  bool FindTableName(const void* addr, const char** name) const;
  unsigned int GetRevisionNumber() const;
  const void* GetImage() const;
  unsigned int GetImageSize() const;

  // ---- PC accessors (see the comment at the top) ----
  static unsigned int U32(const void* p, unsigned int off = 0) { return simmy::be32(At(p, off)); }
  static int S32(const void* p, unsigned int off = 0) { return static_cast<int>(U32(p, off)); }
  static unsigned short U16(const void* p, unsigned int off = 0) { return simmy::be16(At(p, off)); }
  static short S16(const void* p, unsigned int off = 0) { return static_cast<short>(U16(p, off)); }
  static unsigned char U8(const void* p, unsigned int off = 0) { return *At(p, off); }
  static float F32(const void* p, unsigned int off = 0) { return simmy::befloat(At(p, off)); }
  static const void* Ptr(const void* p, unsigned int off = 0) {
    const int rel = S32(p, off);
    return rel ? At(p, off) + rel : nullptr;
  }
  static const char* String(const void* p, unsigned int off = 0) { return static_cast<const char*>(Ptr(p, off)); }
  static const unsigned short* const* LocSlot(const void* p, unsigned int off = 0) {
    return static_cast<const unsigned short* const*>(Ptr(p, off));
  }
  static const unsigned short* LocString(const void* p, unsigned int off = 0) {
    const unsigned short* const* slot = LocSlot(p, off);
    return slot ? *slot : nullptr;
  }
  static const char* TableName(const void* table) { return String(table, kTableName); }
  static const unsigned char* TableRows(const void* table) {
    return static_cast<const unsigned char*>(Ptr(table, kTableRows));
  }
  static const void* TableRowNames(const void* table) { return Ptr(table, kTableRowNames); }
  static const char* TableRowName(const void* table, unsigned int row) {
    const void* names = TableRowNames(table);
    return names ? String(names, row * 4) : nullptr;
  }
  static unsigned int TableRowCount(const void* table) { return U32(table, kTableRowCount); }
  static unsigned int TableRowSize(const void* table) { return U32(table, kTableRowSize); }
  static const void* TableRow(const void* table, unsigned int row) {
    const unsigned int stride = TableRowSize(table) ? TableRowSize(table) : 1;
    return TableRows(table) ? TableRows(table) + row * stride : nullptr;
  }
  // Table count and the i-th table (PC helpers for tools; the original read the image directly).
  int GetTableCount() const { return m_image ? S32(m_image, kImageTableCount) : 0; }
  const void* GetTableByIndex(int i) const { return m_image + kImageTables + i * kTableEntrySize; }

  unsigned char* m_image = nullptr;  // +0x14, the XImage (PC: allocation also holds the slot cells)
  unsigned int m_imageSize = 0;      // +0x18, image + largest language blob, as on the GameCube
  unsigned int m_cellCount = 0;      // PC: native slot cells after m_imageSize (8-aligned)

 private:
  static const unsigned char* At(const void* p, unsigned int off) { return static_cast<const unsigned char*>(p) + off; }
};
