// Port of engine/e_fontdata.cpp: the stored form of a font. Version 0 data only (as the original).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

using simmy::read_be;

// ---- EFontCharacter --------------------------------------------------------------------------
// 80364718 EFontCharacter::GetTypeInfo
// 80364724 EFontCharacter::GetTypeName
// 80364730 EFontCharacter::GetTypeKey
// 8036473C EFontCharacter::GetTypeVersion
// 803646D8 EFontCharacter::SafeDelete
// 80364684 EFontCharacter::New
// 803646B8 EFontCharacter::Construct
// 803646C8 EFontCharacter::Destruct
// 80364748 EFontCharacter::GetTypeInfoStatic
// 80364754 EFontCharacter::GetReadVersion
SIMMY_STORABLE_IMPL(EFontCharacter, "EFontCharacter", 0, EStorable)

// 803636E8
void EFontCharacter::Load(EFile& file) {
  if (m_typeInfo.m_readVersion != 0) return;
  read_be(file, m_left);
  read_be(file, m_right);
  read_be(file, m_row);
  read_be(file, m_page);
}

// 80363520
EFile& operator>>(EFile& file, EFontCharacter*& obj) {
  EStorable* p;
  file >> p;
  obj = static_cast<EFontCharacter*>(p);
  return file;
}

// ---- EFontPage -------------------------------------------------------------------------------
// 8036486C EFontPage::GetTypeInfo
// 80364878 EFontPage::GetTypeName
// 80364884 EFontPage::GetTypeKey
// 80364890 EFontPage::GetTypeVersion
// 8036482C EFontPage::SafeDelete
// 803647D4 EFontPage::New
// 803647FC EFontPage::Construct
// 8036481C EFontPage::Destruct
// 8036489C EFontPage::GetTypeInfoStatic
// 803648A8 EFontPage::GetReadVersion
SIMMY_STORABLE_IMPL(EFontPage, "EFontPage", 0, EStorable)

// 80363844
EFontPage::EFontPage() {}

// 803639A8
void EFontPage::Load(EFile& file) {
  DeallocateShader();
  if (m_typeInfo.m_readVersion != 0) return;
  read_be(file, m_shaderId);
  read_be(file, m_width);
  read_be(file, m_height);
}

// 80363A48
void EFontPage::DeallocateShader() {
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
}

// 80363804
EFile& operator>>(EFile& file, EFontPage*& obj) {
  EStorable* p;
  file >> p;
  obj = static_cast<EFontPage*>(p);
  return file;
}

// ---- EFontSize -------------------------------------------------------------------------------
// 803649D4 EFontSize::GetTypeInfo
// 803649E0 EFontSize::GetTypeName
// 803649EC EFontSize::GetTypeKey
// 803649F8 EFontSize::GetTypeVersion
// 80364994 EFontSize::SafeDelete
// 80364928 EFontSize::New
// 80364950 EFontSize::Construct
// 80364970 EFontSize::Destruct
// 80364A04 EFontSize::GetTypeInfoStatic
// 80364A10 EFontSize::GetReadVersion
SIMMY_STORABLE_IMPL(EFontSize, "EFontSize", 0, EStorable)

// 80363B28
EFontSize::EFontSize() {}
// 80363B88
EFontSize::~EFontSize() { Deallocate(); }

// 80363BF8
void EFontSize::Deallocate() {
  for (auto& [code, ch] : m_chars) ch->SafeDelete();
  m_chars.clear();
  DeallocatePages();
}

// 80363C74
void EFontSize::DeallocateShaders() {
  for (EFontPage* page : m_pages) page->DeallocateShader();
}

// 80363CD8
void EFontSize::DeallocatePages() { m_pages.clear(); }

// 80363E68
// u32 size, u32, u32, the glyph table (u32 table size, u32 count, {u32 code, glyph}), u32 page count,
// the pages.
void EFontSize::Load(EFile& file) {
  Deallocate();
  if (m_typeInfo.m_readVersion != 0) return;
  read_be(file, m_size);
  read_be(file, m_cellHeight);
  read_be(file, m_pixelFont);
  simmy::read_u32(file);  // hash table size
  for (unsigned int n = simmy::read_u32(file); n > 0; --n) {
    const unsigned int code = simmy::read_u32(file);
    EFontCharacter* ch;
    file >> ch;
    m_chars[code] = ch;
  }
  m_pages.resize(simmy::read_u32(file));
  for (EFontPage*& page : m_pages) file >> page;
}

// 80363AE8
EFile& operator>>(EFile& file, EFontSize*& obj) {
  EStorable* p;
  file >> p;
  obj = static_cast<EFontSize*>(p);
  return file;
}

// ---- EFontData -------------------------------------------------------------------------------
// 80364B3C EFontData::GetTypeInfo
// 80364B48 EFontData::GetTypeName
// 80364B54 EFontData::GetTypeKey
// 80364B60 EFontData::GetTypeVersion
// 80364AFC EFontData::SafeDelete
// 80364A90 EFontData::New
// 80364AB8 EFontData::Construct
// 80364AD8 EFontData::Destruct
// 80364B6C EFontData::GetTypeInfoStatic
// 80364B78 EFontData::GetReadVersion
SIMMY_STORABLE_IMPL(EFontData, "EFontData", 0, EStorable)

// 80364024
EFontData::EFontData() {}
// 80364098
EFontData::~EFontData() { Deallocate(); }

// 80364104
void EFontData::Deallocate() {
  if (m_ownsSizes)
    for (EFontSize* size : m_sizes) size->SafeDelete();
  m_sizes.clear();
  m_table.clear();
}

// 803643E0
void EFontData::Load(EFile& file) {
  Deallocate();
  if (m_typeInfo.m_readVersion != 0) return;
  for (unsigned int n = simmy::read_u32(file); n > 0; --n) {
    EFontSize* size;
    file >> size;
    m_sizes.push_back(size);
  }
  read_be(file, m_lineBottom);
  read_be(file, m_cellRaise);
  read_be(file, m_designSize);
  read_be(file, m_defaultKern);
  read_be(file, m_lineSpacing);
  read_be(file, m_missingAdvance);
  simmy::read_u32(file);  // hash table size
  for (unsigned int n = simmy::read_u32(file); n > 0; --n) {
    const unsigned int key = simmy::read_u32(file);
    m_table[key] = static_cast<int>(simmy::read_u32(file));
  }
}

// 80363FE4
EFile& operator>>(EFile& file, EFontData*& obj) {
  EStorable* p;
  file >> p;
  obj = static_cast<EFontData*>(p);
  return file;
}
