// Port of engine/e_ngcmemorycard.cpp (and EMemoryCard's inline defaults): saves through the CARD API.
//
// Every SDK call's result goes through the same table (CardResult). Each operation probes the slot,
// mounts it (a "broken" card still mounts so CARDCheck can report it), works, and unmounts.
#include "simmy/types_gen.hpp"
#include "simmy/perf.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static ENgcMemoryCard _ngcMemCard;
EMemoryCard* _pMemoryCard = nullptr;  // 804FF210
int _ngcMemCardCardDead = 0;          // 804FF214

// 8043EEC0: the first 0x4000 bytes written to every save file: two 32-character comment lines, the
// 96x32 RGB5A3 banner, then the 32x32 RGB5A3 icon.
alignas(32) static unsigned char HeaderRGB5A3[0x3840];
static unsigned char* const s_comment1 = HeaderRGB5A3;
static unsigned char* const s_comment2 = HeaderRGB5A3 + 0x20;
static unsigned char* const s_banner = HeaderRGB5A3 + 0x40;    // 0x1800 bytes
static unsigned char* const s_icon = HeaderRGB5A3 + 0x1840;    // 0x800 bytes used
alignas(32) static unsigned char s_WorkArea[CARD_WORKAREA_SIZE];  // 804C7260

// SDK result -> EA result.
static int CardResult(s32 r) {
  switch (r) {
  case CARD_RESULT_READY: return 1;
  case CARD_RESULT_WRONGDEVICE: return -9;
  case CARD_RESULT_NOCARD: return -1;
  case CARD_RESULT_NOFILE: return -7;
  case CARD_RESULT_IOERROR: _ngcMemCardCardDead = 1; return -8;
  case CARD_RESULT_BROKEN: return -4;
  case CARD_RESULT_INSSPACE:
  case CARD_RESULT_LIMIT: return -3;
  case CARD_RESULT_ENCODING: return -6;
  case CARD_RESULT_CANCELED: return -5;
  default: return 0;  // BUSY, EXIST, NOENT, NOPERM, NAMETOOLONG, FATAL_ERROR
  }
}

// Probes until the slot is not busy.
static int Probe(s32 chan, s32* memSize = nullptr, s32* sectorSize = nullptr) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  s32 mem = 0, sector = 0, r;
  do r = CARDProbeEx(chan, &mem, &sector);
  while (r == CARD_RESULT_BUSY);
  if (memSize) *memSize = mem;
  if (sectorSize) *sectorSize = sector;
  return CardResult(r);
}

// CARDUnmount. Aurora's is a placeholder that always reports CARD_RESULT_NOCARD (it keeps no mount
// state), which would fail every operation after its work is done; an unmount cannot fail otherwise.
static s32 Unmount(s32 chan) {
  ::CARDUnmount(chan);
  return CARD_RESULT_READY;
}

// CARDMount then CARDCheck. A broken card mounts, and CARDCheck then reports (or repairs) it.
static int MountAndCheck(s32 chan) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  const s32 r = CARDMount(chan, s_WorkArea, nullptr);
  if (r != CARD_RESULT_READY && r != CARD_RESULT_BROKEN) return CardResult(r);
  return CardResult(CARDCheck(chan));
}

// 8032FE7C
ENgcMemoryCard::ENgcMemoryCard() { _pMemoryCard = this; }
// 8032FE9C
ENgcMemoryCard::~ENgcMemoryCard() {}
// 803C7668
EMemoryCard::~EMemoryCard() {}

// 8032FED0
int ENgcMemoryCard::IsCardOperationInProgress() { return m_operations != 0; }
// 8032FEE8
void ENgcMemoryCard::BeginCardOperation() { ++m_operations; }
// 8032FEF8
void ENgcMemoryCard::EndCardOperation() { --m_operations; }

// 8032FF08
// The comment lines are UCS-2; the card stores one byte per character (™ becomes 0x99).
void ENgcMemoryCard::SetComments(unsigned short* line1, unsigned short* line2) {
  for (auto [src, dst] : {std::pair{line1, s_comment1}, std::pair{line2, s_comment2}}) {
    unsigned int i = 0;
    for (; i < 32 && src[i]; ++i) dst[i] = src[i] == 0x2122 ? 0x99 : static_cast<unsigned char>(src[i]);
    dst[i < 32 ? i : 31] = 0;
  }
}

// 80334E9C / 80334F2C / 80335054  LoadTexPalette, UnpackTexPalette, TexGet
// The original loaded a TPL ("texture palette", version 0x0020AF30) from the disc and turned its offsets
// into pointers in place. Here: read the file and copy out the first texture's pixel data.
static bool LoadFirstTplTexture(const char* name, unsigned char* dst, unsigned int size) {
  std::vector<unsigned char> tpl;
  if (!simmy::data_read_file(name, tpl)) return false;
  auto at = [&](size_t off) { return off + 4 <= tpl.size() ? simmy::be32(tpl.data() + off) : 0u; };
  if (at(0) != 0x0020AF30) {
    std::fprintf(stderr, "[memcard] %s: invalid version number for texture palette\n", name);
    return false;
  }
  const unsigned int header = at(at(8));  // descriptor[0].textureHeader
  const unsigned int data = at(header + 8);
  if (!header || data + size > tpl.size()) return false;
  std::memcpy(dst, tpl.data() + data, size);
  return true;
}

// 80330020
int ENgcMemoryCard::InitMemoryCard() {
  BeginCardOperation();
  CARDInit("G4ZE", "69");  // the PC CARD needs the game and maker codes that a disc would provide
  EndCardOperation();
  if (!LoadFirstTplTexture("ngcicon.tpl", s_icon, 0x800)) std::memset(s_icon, 0x7f, 0x800);
  if (!LoadFirstTplTexture("ngcbanner.tpl", s_banner, 0x1800)) std::memset(s_banner, 0x7f, 0x1800);
  _ngcMemCardCardDead = 0;
  return 1;
}

// 803301BC
int ENgcMemoryCard::UnInitMemoryCard() { return 1; }
// 80330D70
int ENgcMemoryCard::OpenForIO(char*, unsigned int) { return 1; }
// 80330D78
int ENgcMemoryCard::CloseForIO(char*, unsigned int) { return 1; }

// 803301C4
int ENgcMemoryCard::LoadDataChunkS(char* name, unsigned int card, unsigned int size, unsigned int offset, void* dst) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) r = MountAndCheck(card);
  if (r == 1) {
    CARDFileInfo file;
    r = CardResult(CARDOpen(card, name, &file));
    if (r == 1) r = CardResult(CARDRead(&file, dst, size, offset));
    if (r == 1) r = CardResult(CARDClose(&file));
    if (r == 1) r = CardResult(Unmount(card));
    else Unmount(card);
  }
  EndCardOperation();
  return r;
}

// 8033079C
int ENgcMemoryCard::LoadDataS(char* name, unsigned int card, unsigned int size, void* dst) {
  return LoadDataChunkS(name, card, size, 0, dst);
}

// 80330D80
// Writes `size` bytes at `offset`. CARDWrite works in whole sectors, so unaligned ends are
// read-modify-written through a one-sector buffer.
int ENgcMemoryCard::SaveDataChunkS(char* name, unsigned int card, unsigned int size, unsigned int offset, void* src) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  s32 sectorSize = 0;
  int r = Probe(card, nullptr, &sectorSize);
  CARDFileInfo file;
  bool open = false;
  if (r == 1) r = MountAndCheck(card);
  if (r == 1) {
    r = CardResult(CARDOpen(card, name, &file));
    open = r == 1;
  }
  if (r == 1) {
    const unsigned int sector = static_cast<unsigned int>(sectorSize);
    if (size % sector || offset % sector) {
      std::vector<unsigned char> block(sector);
      const unsigned char* from = static_cast<const unsigned char*>(src);
      int left = static_cast<int>(size);
      while (left > 0 && r == 1) {
        const unsigned int base = offset / sector * sector;
        if (base != offset || left < static_cast<int>(sector))
          r = CardResult(CARDRead(&file, block.data(), sector, base));
        if (r != 1) break;
        unsigned int n = sector - (offset - base);
        if (static_cast<int>(n) > left) n = left;
        std::memcpy(block.data() + (offset - base), from, n);
        r = CardResult(CARDWrite(&file, block.data(), sector, base));
        offset += n;
        from += n;
        left -= static_cast<int>(n);
      }
    } else {
      r = CardResult(CARDWrite(&file, src, size, offset));
    }
  }
  if (r == 1) {
    r = CardResult(CARDClose(&file));
    if (r == 1) r = CardResult(Unmount(card));
  } else {
    if (open) CARDClose(&file);
    Unmount(card);
  }
  EndCardOperation();
  return r;
}

// 80331608
// Creates the file if needed and writes `size` bytes from the start. (The GameCube copied data that
// lived in ARAM-backed virtual memory through a sector buffer first; host memory needs no copy.)
int ENgcMemoryCard::SaveDataS(char* name, unsigned int card, unsigned int size, void* src) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) r = MountAndCheck(card);
  CARDFileInfo file;
  bool open = false;
  if (r == 1) {
    if (CARDCreate(card, name, size, &file) == CARD_RESULT_IOERROR) _ngcMemCardCardDead = 1;
    r = CardResult(CARDOpen(card, name, &file));
    open = r == 1;
  }
  if (r == 1) r = CardResult(CARDWrite(&file, src, size, 0));
  if (open) {
    const int c = CardResult(CARDClose(&file));
    if (r == 1) r = c;
  }
  if (r == 1) r = CardResult(Unmount(card));
  else Unmount(card);
  EndCardOperation();
  return r;
}

// 80331E0C
// Creates a save file of exactly `size` bytes (recreating it if one of another size exists), then
// writes the banner, icon and comments into its first 0x4000 bytes.
int ENgcMemoryCard::CreateFileS(char* name, unsigned int card, unsigned int size) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) r = MountAndCheck(card);
  if (r == 1) {
    CARDFileInfo file;
    const s32 created = CARDCreate(card, name, size, &file);
    if (created == CARD_RESULT_EXIST) {
      r = CardResult(CARDOpen(card, name, &file));
      CARDStat stat;
      if (r == 1) r = CardResult(CARDGetStatus(card, file.fileNo, &stat));
      if (r == 1 && stat.length != size) {
        r = CardResult(CARDClose(&file));
        if (r == 1) r = CardResult(CARDDelete(card, name));
        if (r == 1) r = CardResult(CARDCreate(card, name, size, &file));
      }
    } else {
      r = CardResult(created);
    }
    if (r != 1) Unmount(card);
    else r = CardResult(Unmount(card));
  }
  if (r == 1) r = CheckAndWriteBannerAndIcon(name, card);
  EndCardOperation();
  return r;
}

// 803326B4
int ENgcMemoryCard::CheckAndWriteBannerAndIcon(char* name, unsigned int card) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) r = MountAndCheck(card);
  CARDFileInfo file;
  if (r == 1) {
    r = CardResult(CARDOpen(card, name, &file));
    if (r != 1) Unmount(card);
  }
  if (r == 1) {
    CARDStat stat{};
    s32 s;
    do s = CARDGetStatus(card, file.fileNo, &stat);
    while (s == CARD_RESULT_BUSY);
    if (s != CARD_RESULT_READY) {
      r = 0;
      Unmount(card);
    } else {
      stat.bannerFormat = (stat.bannerFormat & 0xfa) | CARD_STAT_BANNER_RGB5A3;
      stat.iconFormat = (stat.iconFormat & 0xfc02) | CARD_STAT_ICON_RGB5A3;  // one icon frame
      stat.iconSpeed = (stat.iconSpeed & 0xfff3) | CARD_STAT_SPEED_SLOW;
      stat.iconAddr = 0x40;
      stat.commentAddr = 0;
      std::vector<unsigned char> header(0x4000), current(0x4000);
      std::memcpy(header.data(), HeaderRGB5A3, sizeof HeaderRGB5A3);
      s = CARDRead(&file, current.data(), 0x4000, 0);
      if (s >= 0 && header != current) s = CARDWrite(&file, header.data(), 0x4000, 0);
      if (s < 0) {
        Unmount(card);
        r = CardResult(s);
      } else {
        CARDSetStatus(card, file.fileNo, &stat);
        r = CardResult(Unmount(card));
      }
    }
  }
  EndCardOperation();
  return r;
}

// Mount for delete/format: also proceeds when the card's character encoding does not match.
static int MountForMaintenance(s32 chan) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  const s32 r = CARDMount(chan, s_WorkArea, nullptr);
  return r == CARD_RESULT_READY || r == CARD_RESULT_BROKEN || r == CARD_RESULT_ENCODING ? 1 : CardResult(r);
}

// 80332CE4
int ENgcMemoryCard::DeleteDataS(char* name, unsigned int card) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) r = MountForMaintenance(card);
  if (r == 1) {
    r = CardResult(CARDDelete(card, name));
    const int u = CardResult(Unmount(card));
    if (r == 1) r = u;
  }
  EndCardOperation();
  return r;
}

// 80332FEC
int ENgcMemoryCard::FormatCardS(unsigned int card) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) r = MountForMaintenance(card);
  if (r == 1) {
    r = CardResult(CARDFormat(card));
    const int u = CardResult(Unmount(card));
    if (r == 1) r = u;
  }
  EndCardOperation();
  return r;
}

// 803332E0
int ENgcMemoryCard::UnFormatCardS(unsigned int) { return 0; }
// 803332E8
int ENgcMemoryCard::LoadDataA(char*, unsigned int, unsigned int, void*, EMC_Operation*) { return 0; }
// 803332F0
int ENgcMemoryCard::SaveDataA(char*, unsigned int, unsigned int, void*, EMC_Operation*) { return 0; }
// 803332F8
int ENgcMemoryCard::DeleteDataA(char*, unsigned int, unsigned int, EMC_Operation*) { return 0; }
// 80333300
int ENgcMemoryCard::FormatCardA(unsigned int, EMC_Operation*) { return 0; }
// 80333308
int ENgcMemoryCard::UnFormatCardA(unsigned int, EMC_Operation*) { return 0; }
// 80333310
int ENgcMemoryCard::UpdateOperation(EMC_Operation, bool&) { return 0; }

// 80333318
int ENgcMemoryCard::GetFreeSpaceS(unsigned int card, unsigned int& bytes) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) {
    r = MountAndCheck(card);
    s32 freeBytes = 0;
    s32 unusedFiles = 0;  // PC: aurora's CARDFreeBlocks needs both outputs
    if (r == 1) r = CardResult(CARDFreeBlocks(card, &freeBytes, &unusedFiles));
    if (r == 1) bytes = static_cast<unsigned int>(freeBytes);
    Unmount(card);
  }
  EndCardOperation();
  return r;
}

// 80333784
int ENgcMemoryCard::GetFreeFilesS(unsigned int card, int& files) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) {
    r = MountAndCheck(card);
    s32 freeFiles = 0;
    s32 unusedBytes = 0;  // PC: aurora's CARDFreeBlocks needs both outputs
    if (r == 1) r = CardResult(CARDFreeBlocks(card, &unusedBytes, &freeFiles));
    if (r == 1) files = freeFiles;
    Unmount(card);
  }
  EndCardOperation();
  return r;
}

// 80333BF0
void ENgcMemoryCard::IsSpaceAvailable(unsigned int card, unsigned int bytes, bool& available) {
  unsigned int freeBytes = 0;
  GetFreeSpaceS(card, freeBytes);
  available = freeBytes >= bytes;
}

// 80333C5C
void ENgcMemoryCard::IsEnoughFiles(unsigned int card, unsigned int files, bool& enough) {
  int freeFiles = 0;
  GetFreeFilesS(card, freeFiles);
  enough = static_cast<unsigned int>(freeFiles) >= files;
}

// 80333CB4
int ENgcMemoryCard::IsSpaceAvailable(unsigned int, EMC_SaveType, bool& available) {
  available = false;
  return 1;
}

// 80333CC4
int ENgcMemoryCard::CheckForOverwriteSpace(unsigned int, unsigned int, EMC_SaveType, bool& ok) {
  ok = false;
  return 1;
}

// 80333CD4
// A card is present when it answers, even if it is the wrong kind of device.
int ENgcMemoryCard::IsCardAvailable(unsigned int card) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  const int r = Probe(card);
  EndCardOperation();
  return r == 1 || r == -9;
}

// 80333E40
bool ENgcMemoryCard::IsWrongDevice(unsigned int card) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  const int r = Probe(card);
  EndCardOperation();
  return r == -9;
}

// 80333FA0
// Prefers slot A: sets `card` to the lowest present slot and leaves it alone when none is present.
bool ENgcMemoryCard::AnyCardsPresent(int& card) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  if (IsCardAvailable(1)) card = 1;
  if (IsCardAvailable(0)) card = 0;
  return card != -1;
}

// 80334040
int ENgcMemoryCard::IsCardFormated(unsigned int card, bool& formatted) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) {
    const s32 m = CARDMount(card, s_WorkArea, nullptr);
    bool ok = false;
    if (m == CARD_RESULT_READY || m == CARD_RESULT_BROKEN) {
      const s32 c = CARDCheck(card);
      ok = c == CARD_RESULT_READY;
      r = CardResult(c);
    } else {
      r = CardResult(m);
    }
    Unmount(card);
    formatted = ok;
  }
  EndCardOperation();
  return r;
}

// 803343E8
// Lists this game's files (up to `max`). Returns the result of the last directory entry examined.
int ENgcMemoryCard::GetFileList(unsigned int card, char*, unsigned int max, char (*names)[32]) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) r = MountAndCheck(card);
  if (r == 1) {
    unsigned int count = 0;
    for (s32 fileNo = 0; count != max && fileNo < CARD_MAX_FILE; ++fileNo) {
      CARDStat stat;
      r = CardResult(CARDGetStatus(card, fileNo, &stat));
      if (r == 1) {
        std::strncpy(names[count], stat.fileName, 31);
        names[count][31] = 0;
        ++count;
      }
    }
    Unmount(card);
  }
  EndCardOperation();
  return r;
}

// 80334888
int ENgcMemoryCard::GetFileListA(unsigned int, char*, unsigned int, char (*)[32], EMC_Operation*) { return 1; }
// 80334890
void ENgcMemoryCard::SetupSaveTypes(EMC_SaveType, unsigned int) {}
// 80334894
void ENgcMemoryCard::SetGameCode(char*) {}

// 80334898
int ENgcMemoryCard::DoesFileExist(char* name, unsigned int card, bool& exists) {
  SIMMY_PERF_SCOPE(Card);  // PC: the frame-hitch profiler (perf.hpp)
  exists = false;
  BeginCardOperation();
  int r = Probe(card);
  if (r == 1) r = MountAndCheck(card);
  if (r == 1) {
    // GC: scanned the directory with CARDGetStatus (every empty slot answering NOFILE, which aurora
    // logs). PC: open by name instead; the result is the same (1 found, NOFILE's -7 not).
    CARDFileInfo info;
    r = CardResult(CARDOpen(static_cast<s32>(card), name, &info));
    if (r == 1) {
      exists = true;
      CARDClose(&info);
    }
    Unmount(card);
  }
  EndCardOperation();
  return r;
}

// 80334D38
int ENgcMemoryCard::GetSectorSize(int card, unsigned int& size) {
  BeginCardOperation();
  s32 sector = 0;
  const int r = Probe(card, nullptr, &sector);
  EndCardOperation();
  size = static_cast<unsigned int>(sector);
  return r;
}

// EMemoryCard's inline defaults.
// 803C769C
int EMemoryCard::OpenForIO(char*, unsigned int) { return 1; }
// 803C76A4
int EMemoryCard::CloseForIO(char*, unsigned int) { return 1; }
// 803C76AC
int EMemoryCard::GetFileRefCount(char*, unsigned int) { return 0; }
// 803C76B4
int EMemoryCard::LoadDataChunkS(char*, unsigned int, unsigned int, unsigned int, void*) { return 1; }
// 803C76BC
int EMemoryCard::SaveDataChunkS(char*, unsigned int, unsigned int, unsigned int, void*) { return 1; }
// 803C76C4
int EMemoryCard::CreateFileS(char*, unsigned int, unsigned int) { return 1; }
// 803C76CC
void EMemoryCard::SetMemCardCopyProtection(unsigned int) {}
// 803C76D0
int EMemoryCard::PollCards(bool) { return 1; }
// 803C76D8
int EMemoryCard::IsPolledCardAvailable(unsigned int) { return 1; }
// 803C76E0
bool EMemoryCard::IsWrongDevice(unsigned int) { return false; }
