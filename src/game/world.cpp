// Port of game/world.cpp (cFixedWorldImpl, partial), game/newarray2d.cpp (_c2DArray, partial) and
// game/reconbuffer.cpp (ReconBuffer, partial): the lot's tile arrays read from the house's resources.
// Still stubbed: room computation, walls and floors
// edited by build mode, the influence maps.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/placement.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

cFixedWorldImpl* Globs_pFixedWorld = nullptr;  // 804FDBF8 Globs::pFixedWorld

namespace {
// The arrays' name (DAT_803DDE2C, an empty string).
BString& EmptyName() {
  static BString name;
  return name;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// ReconBuffer

// 80132D9C
ReconBuffer::ReconBuffer(void* data, int size, ReconBuffer__Mode mode, bool) {
  m_data = static_cast<unsigned char*>(data);
  m_size = size;
  m_mode = mode;
  m_littleEndian = 1;
  m_pos = 0;
  m_field14 = 0;
  m_compressed = 0;
  m_field1C = m_field20 = 0;
  m_bitMask = 0;
  m_mark = 0;
}

// 80132E18
ReconBuffer::~ReconBuffer() {}

// 80132F40
void ReconBuffer::EnableCompression() { m_compressed = 1; }

void PcPrintBacktrace();  // recon.cpp
// PC dev aid (SIMMY_RECON_TRACE, ReconBuilder::Compact): each value op's starting position.
std::vector<int>* g_reconOpTrace = nullptr;
int g_reconOpStop = -1;  // the op to print a backtrace at
void ReconTraceOp(const ReconBuffer& b) {
  if (!g_reconOpTrace) return;
  g_reconOpTrace->push_back(b.m_pos);
  if (static_cast<int>(g_reconOpTrace->size()) - 1 == g_reconOpStop) PcPrintBacktrace();
}

// 801332C4
void ReconBuffer::PadBits() { m_bitMask = 0; }

// 80133108
// `bits` bits of *v, most significant first, packed from the high bit of each byte; m_pos is past
// the byte being filled.
void ReconBuffer::ReconBits(int bits, int* v) {
  if (m_mode == ReconBuffer__Mode::kMeasure) {
    for (; bits > 7; bits -= 8) ++m_pos;
    for (; bits > 0; --bits) {
      if (m_bitMask == 0) {
        m_bitMask = 0x80;
        ++m_pos;
      }
      m_bitMask /= 2;
    }
    return;
  }
  unsigned int mask = static_cast<unsigned int>(m_bitMask);
  unsigned char* p = m_data + m_pos - 1;
  auto& value = *reinterpret_cast<unsigned int*>(v);
  if (m_mode == ReconBuffer__Mode::kRead) value = 0;
  for (unsigned int bit = 1u << (bits - 1); bit; bit >>= 1) {
    if (mask == 0) {
      ++p;
      if (m_mode == ReconBuffer__Mode::kWrite) *p = 0;
      mask = 0x80;
    }
    if (m_mode == ReconBuffer__Mode::kWrite) {
      if (value & bit) *p |= static_cast<unsigned char>(mask);
    } else if (*p & mask) {
      value |= bit;
    }
    mask >>= 1;
  }
  m_bitMask = static_cast<int>(mask);
  m_pos = static_cast<int>(p + 1 - m_data);
}

// 80132F4C
// Written: a "non-zero" bit, then the narrowest of the scheme's widths that holds the value
// sign-extended, as 2 bits, and the value in that width. Read back the same way.
void ReconBuffer::ReconCmprInt(int* v, const Scheme* scheme) {
  if (m_mode == ReconBuffer__Mode::kWrite || m_mode == ReconBuffer__Mode::kMeasure) {
    int nonZero = *v != 0;
    ReconBits(1, &nonZero);
    if (!nonZero) return;
    const unsigned int value = static_cast<unsigned int>(*v);
    int width = 0;
    while (width < 3) {
      const unsigned int high = value & scheme->widths[width].mask;
      if (high == scheme->widths[width].mask || high == 0) break;
      ++width;
    }
    ReconBits(2, &width);
    ReconBits(scheme->widths[width].bits, v);
    return;
  }
  int nonZero = 0;
  ReconBits(1, &nonZero);
  if (!nonZero) {
    *v = 0;
    return;
  }
  int width = 0;
  ReconBits(2, &width);
  ReconBits(scheme->widths[width].bits, v);
  const unsigned int mask = scheme->widths[width].mask;
  if (static_cast<unsigned int>(*v) & mask) *v = static_cast<int>(static_cast<unsigned int>(*v) | mask);
}

namespace {
// 80134778 (static initialisation): the widths for 8, 16 and 32 bit values.
const Scheme scheme8 = {{{2, 0xfffffffeu}, {4, 0xfffffff8u}, {6, 0xffffffe0u}, {8, 0xffffff80u}}};
const Scheme scheme16 = {{{5, 0xfffffff0u}, {8, 0xffffff80u}, {13, 0xfffff000u}, {16, 0xffff8000u}}};
const Scheme scheme32 = {{{6, 0xffffffe0u}, {11, 0xfffffc00u}, {21, 0xfff00000u}, {32, 0x80000000u}}};

// Reads (mode 0) or writes (1) `count` words of `width` bytes, swapping when the buffer's byte order
// is not the machine's (PC: little-endian); mode 2 only counts. Compressed buffers code each word
// with ReconCmprInt.
void ReconWords(ReconBuffer& b, void* v, int width, int count) {
  ReconTraceOp(b);
  auto* p = static_cast<unsigned char*>(v);
  if (b.m_compressed) {
    const Scheme* scheme = width == 1 ? &scheme8 : width == 2 ? &scheme16 : &scheme32;
    for (int i = 0; i < count; ++i, p += width) {
      int value = width == 1 ? *reinterpret_cast<signed char*>(p)
                             : width == 2 ? *reinterpret_cast<short*>(p) : *reinterpret_cast<int*>(p);
      b.ReconCmprInt(&value, scheme);
      if (b.m_mode != ReconBuffer__Mode::kRead) continue;
      if (width == 1) *reinterpret_cast<signed char*>(p) = static_cast<signed char>(value);
      else if (width == 2) *reinterpret_cast<short*>(p) = static_cast<short>(value);
      else *reinterpret_cast<int*>(p) = value;
    }
    return;
  }
  if (b.m_mode == ReconBuffer__Mode::kMeasure) {
    b.m_pos += width * count;
    return;
  }
  for (int i = 0; i < count; ++i, p += width, b.m_pos += width) {
    unsigned char* buf = b.m_data + b.m_pos;
    for (int k = 0; k < width; ++k) {
      const int bk = b.m_littleEndian ? k : width - 1 - k;
      if (b.m_mode == ReconBuffer__Mode::kRead) p[k] = buf[bk];
      else buf[bk] = p[k];
    }
  }
}
}  // namespace

// 801332D0
void ReconBuffer::Recon8(signed char* v, int count) { ReconWords(*this, v, 1, count); }
// 80133430
void ReconBuffer::ReconBool(int* v) {
  signed char c = static_cast<signed char>(*v);
  Recon8(&c, 1);
  *v = c != 0;
}
// 80133484
void ReconBuffer::Recon16(short* v, int count) { ReconWords(*this, v, 2, count); }
// 80133614
void ReconBuffer::ReconInt(int* v, int count) { Recon32(v, count); }
// 80133634
void ReconBuffer::Recon32(int* v, int count) { ReconWords(*this, v, 4, count); }

// 8013398C
// A mark: a word holding the position of the next mark (written when that mark is), so a reader can
// skip what it does not understand (ReadToNextMark).
void ReconBuffer::ReconMark() {
  ReconTraceOp(*this);
  if (m_compressed) PadBits();
  if (m_mode == ReconBuffer__Mode::kWrite) {
    auto put = [&](int at, unsigned int v) {
      for (int k = 0; k < 4; ++k)
        m_data[at + (m_littleEndian ? k : 3 - k)] = static_cast<unsigned char>(v >> (8 * k));
    };
    if (m_mark) put(m_mark, static_cast<unsigned int>(m_pos));
    m_mark = m_pos;
    put(m_pos, 0);
  } else if (m_mode == ReconBuffer__Mode::kRead) {
    unsigned int v = 0;
    for (int k = 0; k < 4; ++k) v |= static_cast<unsigned int>(m_data[m_pos + (m_littleEndian ? k : 3 - k)]) << (8 * k);
    m_mark = static_cast<int>(v);
  }
  m_pos += 4;
}

// 80133A98
void ReconBuffer::ReadToNextMark() {
  if (m_compressed) PadBits();
  if (m_mode == ReconBuffer__Mode::kRead) m_pos = m_mark;
}

// ---------------------------------------------------------------------------------------------------
// ReconObject, ReconBuilder

// 80134734
ReconObject::~ReconObject() {}
// 80134768
void ReconObject::DoStream(ReconBuffer*, int) {}
// 8013476C
int ReconObject::GetType() { return 0x52636f6e; }  // 'Rcon'

// 80134518
// The resource of the object's type: 0 when read, else the file's error.
int ReconBuilder::Reconstitute(ReconObject* object, iResFile* file, short id, int* version) {
  Memory__HandleNode* handle = file->GetByID(object->GetType(), id, nullptr);
  if (!handle) return file->GetError() ? file->GetError() : -1;
  Reconstitute(object, handle, version);
  return 0;
}

// 801345C0
// The header (GC: byte-swapped from the saves' little-endian) then the stream.
void ReconBuilder::Reconstitute(ReconObject* object, Memory__HandleNode* handle, int* version) {
  const unsigned char* h = handle->m_data;
  const int v = static_cast<int>(h[4] | h[5] << 8 | h[6] << 16 | static_cast<unsigned int>(h[7]) << 24);
  ReconBuffer buffer(handle->m_data + 0xc, static_cast<int>(handle->m_size), ReconBuffer__Mode{ReconBuffer__Mode::kRead},
                     h[0] != 0);
  if (version) *version = v;
  object->DoStream(&buffer, v);
}

// ---------------------------------------------------------------------------------------------------
// _c2DArray

// 800D1B1C
_c2DArray::_c2DArray(int elemSize, int xSize, int ySize, BString& name) {
  m_elemSize = elemSize;
  m_xSize = -1;
  m_rows = nullptr;
  m_ySize = -1;
  AddArray(this);
  SetName(name);
  SetSize(xSize, ySize);
  ClearBytes(0);
}

// 800D1BA8
_c2DArray::~_c2DArray() {
  delete[] reinterpret_cast<unsigned char*>(m_rows);
  RemoveArray(this);
  m_elemSize = 0;
  m_rows = nullptr;
  m_xSize = m_ySize = -1;
}

// 800D1E78
int _c2DArray::ClearBytes(signed char value) {
  if (m_rows && m_xSize > 0 && m_ySize > 0)
    std::memset(m_rows[0], static_cast<unsigned char>(value), static_cast<size_t>(m_xSize) * m_ySize * m_elemSize);
  return 0;
}

// 800D1EC4
// `xSize` rows of `ySize` elements; one block holds the row pointers and the rows.
int _c2DArray::SetSize(int xSize, int ySize) {
  if (ySize == m_ySize && xSize == m_xSize) {
    if (m_rows) return 1;
  } else if (m_rows) {
    delete[] reinterpret_cast<unsigned char*>(m_rows);
    m_rows = nullptr;
  }
  m_ySize = ySize;
  m_xSize = xSize;
  if (xSize < 1 || ySize < 1) {
    m_rows = nullptr;
    return 1;
  }
  const size_t pointers = static_cast<size_t>(xSize) * sizeof(unsigned char*);
  auto* block = new unsigned char[pointers + static_cast<size_t>(xSize) * ySize * m_elemSize]();
  m_rows = reinterpret_cast<unsigned char**>(block);
  m_rows[0] = block + pointers;
  for (int x = 1; x < xSize; ++x) m_rows[x] = m_rows[x - 1] + ySize * m_elemSize;
  return 1;
}

// 800D1A04
// The array resource `id` of `type`: a stream (run-length) when it starts with a zero word, else the
// GameCube's raw layout (element size, sizes, row table, rows).
int _c2DArray::ReadFromDisk(iResFile* file, int type, short id, void (*swizzle)(void*, int)) {
  Memory__HandleNode* handle = file->GetByID(type, id, swizzle);
  if (!handle) return file->GetError();
  const unsigned char* data = handle->m_data;
  const auto word = [&](int i) {
    const unsigned char* p = data + i * 2;
    return static_cast<short>(file->IsLittleEndian(handle) ? (p[0] | p[1] << 8) : (p[0] << 8 | p[1]));
  };
  if (word(0) == 0) {
    ReconBuffer buffer(handle->m_data, static_cast<int>(handle->m_size), ReconBuffer__Mode{ReconBuffer__Mode::kRead}, false);
    buffer.m_littleEndian = file->IsLittleEndian(handle);
    DoStream(&buffer, false);
  } else {
    const int elemSize = word(0), xSize = word(1), ySize = word(2);
    m_elemSize = elemSize;
    if (!SetSize(xSize, ySize)) return -1;
    const size_t n = static_cast<size_t>(elemSize) * xSize * ySize;
    if (n) std::memcpy(m_rows[0], data + (xSize * 2 + 4) * 2, n);
    file->Release(handle);
  }
  return 0;
}

// 800D1C74
// The array as resource `id` of `type`: streamed (run-length coded when `compress`) into a buffer of
// its size and 0x40 bytes, cut to what was written and added to the file (GC: then the file's Write,
// a no-op for the neighbourhood file). 0: added, else the file's error.
int _c2DArray::WriteToDisk(iResFile* file, int type, short id, bool compress) {
  const unsigned int size = static_cast<unsigned int>(m_xSize * m_ySize * m_elemSize + 0x40);
  std::vector<unsigned char> buf(size, 0);
  ReconBuffer buffer(buf.data(), static_cast<int>(size), ReconBuffer__Mode{ReconBuffer__Mode::kWrite}, false);
  DoStream(&buffer, compress);
  auto* handle = new Memory__HandleNode;
  handle->m_size = static_cast<unsigned int>(buffer.m_pos) < size ? static_cast<unsigned int>(buffer.m_pos) : size;
  handle->m_data = handle->m_size ? new unsigned char[handle->m_size] : nullptr;
  handle->m_owned = 1;
  if (handle->m_size) std::memcpy(handle->m_data, buf.data(), handle->m_size);
  static StringBuffer name;  // GC: the array's name (BString +0x10); the neighbourhood file ignores it
  file->Add(handle, type, id, name, false);
  return file->GetError();
}

// 800D1FF8
// Version words, the sizes, the element size and whether run-length coded; then the elements: runs
// of a count word (high bit: one byte repeated) and the bytes, each padded to an even position.
void _c2DArray::DoStream(ReconBuffer* b, bool compress) {
  short version = 0, version2 = 0;
  b->Recon16(&version, 1);
  b->Recon16(&version2, 1);
  int xSize = m_xSize, ySize = m_ySize, coded = compress ? 1 : 0;
  b->ReconInt(&xSize, 1);
  b->ReconInt(&ySize, 1);
  b->ReconInt(&m_elemSize, 1);
  b->ReconInt(&coded, 1);
  if (b->m_mode == ReconBuffer__Mode::kRead && !SetSize(xSize, ySize)) return;
  if (!m_rows) return;
  auto* p = reinterpret_cast<signed char*>(m_rows[0]);
  auto* const end = reinterpret_cast<signed char*>(m_rows[m_xSize - 1] + m_ySize * m_elemSize);
  if (!coded) {
    b->Recon8(p, static_cast<int>(end - p));
    return;
  }
  if (b->m_mode == ReconBuffer__Mode::kWrite) {
    // Written: up to the next run of more than 7 equal bytes as a literal block, a run as one byte
    // with the count's high bit set (blocks of at most 0x7FFF).
    while (p < end) {
      int run = 0;
      signed char* q = p;
      for (; q < end; q += run) {
        run = 1;
        if (q + 1 < end) {
          signed char c = *q;
          while (q[1] == c && (++run, q + run < end)) c = q[run];
        }
        if (run > 7) break;
      }
      const bool literal = q != p;
      if (literal) run = static_cast<int>(q - p);
      if (run > 0x7fff) run = 0x7fff;
      short count = static_cast<short>(literal ? run : (run | 0x8000));
      b->Recon16(&count, 1);
      b->Recon8(p, literal ? run : 1);
      if (b->m_pos & 1) {
        signed char pad = 0;
        b->Recon8(&pad, 1);
      }
      p += run;
    }
    return;
  }
  if (b->m_mode != ReconBuffer__Mode::kRead) {
    SIMMY_STUB("_c2DArray::DoStream: measuring a run-length coded array", 0x800D1FF8);
    return;
  }
  while (p < end && b->m_pos < b->m_size) {
    short run = 0;
    b->Recon16(&run, 1);
    const int count = run & 0x7fff;
    if (run < 0) {
      b->Recon8(p, 1);
      for (int i = 1; i < count && p + i < end; ++i) p[i] = p[0];
    } else {
      b->Recon8(p, static_cast<int>(count <= end - p ? count : end - p));
    }
    p += count;
    if (b->m_pos & 1) {
      signed char pad;
      b->Recon8(&pad, 1);
    }
  }
}

// ---------------------------------------------------------------------------------------------------
// cFixedWorldImpl

// 8015228C
void CreateTheWorld() { Globs_pFixedWorld = new cFixedWorldImpl(0x27, 0x27); }

// 8014C524
cFixedWorldImpl::cFixedWorldImpl(int xSize, int ySize) {
  SetSize(xSize, ySize, false);
  SIMMY_STUB("cFixedWorldImpl::cFixedWorldImpl: TileWalls::GenerateRotationLookups", 0x8014C524);
}

// 8014C5C0
cFixedWorldImpl::~cFixedWorldImpl() { DeleteArrays(); }

// 8014CAC0
void cFixedWorldImpl::DeleteArrays() {
  delete m_floors;
  delete m_rooms;
  delete m_flags;
  delete m_walls;
  m_floors = m_rooms = m_flags = m_walls = nullptr;
  m_influence = nullptr;  // GC: the InfluenceMap (not ported)
  delete m_passiveInfluence;
  m_passiveInfluence = nullptr;
}

// 80151C30
int cFixedWorldImpl::GetXSize() const { return m_xSize; }
// 80151C38
int cFixedWorldImpl::GetYSize() const { return m_ySize; }
// 80151C40
int cFixedWorldImpl::GetMaxSize() const { return 0x40; }
// 80152364
PassiveInfluenceMap* cFixedWorldImpl::GetPassiveInfluenceMap() { return m_passiveInfluence; }

// 8014C630
// New floor, flag and wall arrays of the size (8 to 64 tiles a side); GC: keeping the old contents
// where they overlap when asked, and new influence maps.
int cFixedWorldImpl::SetSize(int xSize, int ySize, bool keep) {
  if (m_xSize == xSize && m_ySize == ySize) return 1;
  if (xSize > GetMaxSize() || ySize > GetMaxSize() || xSize < 8 || ySize < 8) return 0;
  if (keep) SIMMY_STUB("cFixedWorldImpl::SetSize: keeping the contents (build mode)", 0x8014C630);
  DeleteArrays();
  m_floors = new _c2DArray(1, xSize, ySize, EmptyName());
  m_flags = new _c2DArray(1, xSize, ySize, EmptyName());
  m_walls = new _c2DArray(8, xSize, ySize, EmptyName());
  m_rooms = new _c2DArray(2, xSize, ySize, EmptyName());
  SIMMY_STUB("cFixedWorldImpl::SetSize: the InfluenceMap", 0x8014C630);
  m_passiveInfluence = new PassiveInfluenceMap(xSize, ySize);
  m_xSize = xSize;
  m_ySize = ySize;
  return 1;
}

// 8014CB7C
// The floors ('Arry' 11), walls (12) and flags (8) into the house's file, run-length coded. 0: saved.
// (GC: the walls' result is not checked.)
int cFixedWorldImpl::Save(iResFile* file, int) {
  int r = m_floors->WriteToDisk(file, 0x41727279, 0xb, true);
  if (r == 0) {
    m_walls->WriteToDisk(file, 0x41727279, 0xc, true);
    r = m_flags->WriteToDisk(file, 0x41727279, 8, true);
  }
  return r;
}

// 8014CC0C
// The floors ('Arry' 11), walls (12) and flags (8) of the house's file; a new size makes a new room
// array and influence maps. Then the rooms are computed.
int cFixedWorldImpl::Load(iResFile* file, int) {
  int r = m_floors->ReadFromDisk(file, 0x41727279, 0xb, nullptr);
  if (r == 0) r = m_walls->ReadFromDisk(file, 0x41727279, 0xc, nullptr);
  if (r == 0) r = m_flags->ReadFromDisk(file, 0x41727279, 8, nullptr);
  if (r != 0) return r;
  const int xSize = m_floors->m_xSize, ySize = m_floors->m_ySize;
  if (xSize != m_xSize || ySize != m_ySize) {
    delete m_rooms;
    m_rooms = new _c2DArray(2, xSize, ySize, EmptyName());
    SIMMY_STUB("cFixedWorldImpl::Load: the InfluenceMap", 0x8014CC0C);
    delete m_passiveInfluence;
    m_passiveInfluence = new PassiveInfluenceMap(xSize, ySize);
    m_xSize = xSize;
    m_ySize = ySize;
  }
  return 0;
}

// 8014CDD4
// The tile's walls (none off the lot or when the wall array does not match the lot). The diagonal
// flags say which neighbours (x-1, y-1, y+1, x+1) have diagonals; segments 8 and 4 take the style and
// 80152018
// The tile has any wall segment (its walls' byte 0). GC: no bounds check.
bool cFixedWorldImpl::HasWalls(CTilePt& tile) {
  if (!m_walls || tile.GetX() < 0 || tile.GetY() < 0 || tile.GetX() >= m_xSize || tile.GetY() >= m_ySize) return false;  // PC guard
  return m_walls->At(tile.GetX(), tile.GetY())[0] != 0;
}

// 80151FB0
// The tile has all of `segments`.
bool cFixedWorldImpl::HasWalls(CTilePt& tile, TileWallsSegment segments) {
  if (!m_walls || tile.GetX() < 0 || tile.GetY() < 0 || tile.GetX() >= m_xSize || tile.GetY() >= m_ySize) return false;  // PC guard
  const unsigned int s = static_cast<unsigned int>(segments.value);
  return (m_walls->At(tile.GetX(), tile.GetY())[0] & s) == s;
}

// placement of the next tile's segment 2 (along y) and 1 (along x), which those tiles keep.
TileWalls cFixedWorldImpl::GetWall(CTilePt& tile) {
  const int x = tile.GetX(), y = tile.GetY();
  _c2DArray* walls = m_walls;
  if (!walls || m_xSize != walls->m_xSize || m_ySize != walls->m_ySize || x < 0 || x > m_xSize - 1 || y < 0 ||
      y > m_ySize - 1)
    return TileWalls();
  const bool left = x >= 1 && (walls->At(x - 1, y)[0] & 0x30);
  const bool up = y >= 1 && (walls->At(x, y - 1)[0] & 0x30);
  const bool down = y < m_ySize - 1 && (walls->At(x, y + 1)[0] & 0x30);
  const bool right = x < m_xSize - 1 && (walls->At(x + 1, y)[0] & 0x30);
  TileWalls result(*reinterpret_cast<TileWallStorage*>(walls->At(x, y)), left, up, down, right);
  if (y < m_ySize - 1) {
    const unsigned char* next = walls->At(x, y + 1);
    if (next[0] & 2) {
      result.SetStyle(next[3], 8);
      result.SetPlacement((next[1] >> 2) & 3, 8);
    }
  }
  if (x < m_xSize - 1) {
    const unsigned char* next = walls->At(x + 1, y);
    if (next[0] & 1) {
      result.SetStyle(next[2], 4);
      result.SetPlacement(next[1] & 3, 4);
    }
  }
  return result;
}

// 8014CDC4
_c2DArray* cFixedWorldImpl::GetFloorLayer() { return m_floors; }
// 8014CDCC
_c2DArray* cFixedWorldImpl::GetWalls() { return m_walls; }

// 8014D0C0
// The tile's walls back into its 8-byte entry (TileWallStorage; see tilewalls.hpp), not while the
// placement code is moving an object (s_handlingPlacementObject). A diagonal takes the whole entry (its
// style, the patterns of its two sides and the floor of its two halves). Edge segments 4 and 8 live in
// the next tile's segments 1 and 2 (that tile keeps their style and placement); segments 1 and 2 also set
// the neighbours' bits 4 and 8, and removing them clears those and the pattern stored there. As on GC,
// placements are only ever or'ed in.
void cFixedWorldImpl::SetWall(CTilePt& tile, TileWalls walls) {
  if (g_handlingPlacementObject) return;
  walls.ConvertToWorldCoords();
  const int x = tile.GetX(), y = tile.GetY();
  unsigned char* w = m_walls->At(x, y);
  if (!walls.HasWall(0x10)) {
    w[0] &= 0xef;
  } else {
    walls.HasWall(0x20);
    w[0] = 0x10;
    w[3] = static_cast<unsigned char>(walls.GetStyle(0x10));
    w[6] = static_cast<unsigned char>(walls.GetPattern(0x10, 2));
    w[7] = static_cast<unsigned char>(walls.GetPattern(0x10, 4));
    w[2] = static_cast<unsigned char>(walls.GetFloorValue(2));
    w[4] = static_cast<unsigned char>(walls.GetFloorValue(4));
  }
  if (!walls.HasWall(0x20)) {
    w[0] &= 0xdf;
  } else {
    walls.HasWall(0x10);
    w[0] = 0x20;
    w[3] = static_cast<unsigned char>(walls.GetStyle(0x20));
    w[6] = static_cast<unsigned char>(walls.GetPattern(0x20, 1));
    w[7] = static_cast<unsigned char>(walls.GetPattern(0x20, 3));
    w[2] = static_cast<unsigned char>(walls.GetFloorValue(1));
    w[4] = static_cast<unsigned char>(walls.GetFloorValue(3));
  }
  if (!walls.HasWall(1)) {
    if (w[0] & 1) {
      w[0] &= 0xfe;
      if (x > 0) {
        unsigned char* n = m_walls->At(x - 1, y);
        n[0] &= 0xfb;
        n[7] = 0;
      }
    }
  } else {
    walls.HasDiagonal();
    w[0] |= 1;
    w[2] = static_cast<unsigned char>(walls.GetStyle(1));
    w[4] = static_cast<unsigned char>(walls.GetPattern(1, 0));
    w[1] |= static_cast<unsigned char>(walls.GetPlacement(1));
    if (x > 0) m_walls->At(x - 1, y)[0] |= 4;
  }
  if (!walls.HasWall(2)) {
    if (w[0] & 2) {
      w[0] &= 0xfd;
      if (y > 0) {
        unsigned char* n = m_walls->At(x, y - 1);
        n[0] &= 0xf7;
        n[6] = 0;
      }
    }
  } else {
    walls.HasDiagonal();
    w[0] |= 2;
    w[3] = static_cast<unsigned char>(walls.GetStyle(2));
    w[5] = static_cast<unsigned char>(walls.GetPattern(2, 0));
    w[1] |= static_cast<unsigned char>(walls.GetPlacement(2) << 2);
    if (y > 0) m_walls->At(x, y - 1)[0] |= 8;
  }
  if (!walls.HasWall(4)) {
    if (w[0] & 4) {
      w[0] &= 0xfb;
      if (x < m_xSize - 1) {
        unsigned char* n = m_walls->At(x + 1, y);
        n[0] &= 0xfe;
        n[4] = 0;
      }
    }
  } else {
    walls.HasDiagonal();
    w[0] |= 4;
    w[7] = static_cast<unsigned char>(walls.GetPattern(4, 0));
    if (x < m_xSize - 1) {
      unsigned char* n = m_walls->At(x + 1, y);
      n[0] |= 1;
      n[2] = static_cast<unsigned char>(walls.GetStyle(4));
      n[1] |= static_cast<unsigned char>(walls.GetPlacement(4));
    }
  }
  if (!walls.HasWall(8)) {
    if ((w[0] & 8) && (w[0] &= 0xf7, y < m_ySize - 1)) {
      unsigned char* n = m_walls->At(x, y + 1);
      n[0] &= 0xfd;
      n[5] = 0;
    }
  } else {
    walls.HasDiagonal();
    w[0] |= 8;
    w[6] = static_cast<unsigned char>(walls.GetPattern(8, 0));
    if (y < m_ySize - 1) {
      unsigned char* n = m_walls->At(x, y + 1);
      n[0] |= 2;
      n[3] = static_cast<unsigned char>(walls.GetStyle(8));
      n[1] |= static_cast<unsigned char>(walls.GetPlacement(8) << 2);
    }
  }
}

// 801520D4
void cFixedWorldImpl::SetWallStorage(CTilePt& tile, TileWallStorage& storage) {
  std::memcpy(m_walls->At(tile.GetX(), tile.GetY()), storage.m_bytes, 8);
}

// 80151C48
int cFixedWorldImpl::GetFloor(CTilePt& tile) { return *m_floors->At(tile.GetX(), tile.GetY()); }

// 80151D18
// The tile's floor pattern; the old one.
int cFixedWorldImpl::SetFloor(CTilePt& tile, int pattern) {
  unsigned char* f = m_floors->At(tile.GetX(), tile.GetY());
  const int old = *f;
  if (old != pattern) *f = static_cast<unsigned char>(pattern);
  return old;
}

// 80151A90
// Inside the lot's edge (one tile; two with `inner`).
int cFixedWorldImpl::OutOfBuildingBounds(CTilePt& tile, bool inner) const {
  const int lo = inner ? 2 : 1;
  const int xHi = m_xSize - lo, yHi = m_ySize - lo;
  return !(lo <= tile.m_x && tile.m_x < xHi && lo <= tile.m_y && tile.m_y < yHi);
}

// 8015213C
// Not a tile of the lot's locked part (flag 0x20), unless the move-objects cheat (EGlobal +0x3DC) is on.
int cFixedWorldImpl::MayEditTile(CTilePt& tile) const {
  if (_globals.ConfigInt(0x3dc)) return 1;
  // PC: a tile off the arrays is not editable (GC reads past them).
  if (tile.GetX() < 0 || tile.GetY() < 0 || tile.GetX() >= m_xSize || tile.GetY() >= m_ySize) return 0;
  return ((*m_flags->At(tile.GetX(), tile.GetY()) ^ 0x20) >> 5) & 1;
}

// 80151DA4
// The tile's room (0 off the lot or before the rooms are computed).
unsigned short cFixedWorldImpl::GetRoom(CTilePt& tile) {
  if (!m_rooms || tile.GetX() < 0 || tile.GetX() >= m_rooms->m_xSize || tile.GetY() < 0 ||
      tile.GetY() >= m_rooms->m_ySize)
    return 0;
  return *reinterpret_cast<const unsigned short*>(m_rooms->At(tile.GetX(), tile.GetY()));
}

// 80151E60
void cFixedWorldImpl::SetRoom(CTilePt& tile, unsigned short room) {
  *reinterpret_cast<unsigned short*>(m_rooms->At(tile.GetX(), tile.GetY())) = room;
}

// 80151EB8
unsigned char cFixedWorldImpl::GetFlags(CTilePt& tile) { return *m_flags->At(tile.GetX(), tile.GetY()); }

// 80151F08
void cFixedWorldImpl::SetFlags(CTilePt& tile, unsigned char flags) { *m_flags->At(tile.GetX(), tile.GetY()) = flags; }

// 80151F5C
// Flag 4: reached from the lot's edge without crossing a wall (ComputeRooms).
int cFixedWorldImpl::IsOutside(CTilePt& tile) { return (*m_flags->At(tile.GetX(), tile.GetY()) >> 2) & 1; }

// 80151A40
// The lot's edge row and column count as out of bounds.
int cFixedWorldImpl::OutOfBounds(CTilePt& tile) const {
  return !(tile.m_x > 0 && tile.m_x < m_xSize - 1 && tile.m_y > 0 && tile.m_y < m_ySize - 1);
}

// 80151B10
// Off the lot's grid (the border tiles count as on it).
int cFixedWorldImpl::OutOfGrid(CTilePt& tile) const {
  return !(tile.m_x >= 0 && tile.m_x < m_xSize && tile.m_y >= 0 && tile.m_y < m_ySize);
}

// 801521AC
// The tile's room; a tile split by a diagonal wall answers its first half's room.
unsigned short cFixedWorldImpl::GetRoomIdFromPoint(CTilePt& tile) {
  unsigned short room = GetRoom(tile);
  if (!Globs_pRoomManager) return 0;
  if (room == 0xfffb) {
    unsigned short room1 = 0, room2 = 0;
    Room__Sides side1, side2;
    Globs_pRoomManager->ResolveDiagonal(tile, &room1, &room2, &side1, &side2);
    // GC: also reads the tile's walls (HasWall 0x20, then 0x10) without using the answer.
    room = room1;
  }
  return room;
}

namespace {
// Wall styles 2, 0xC, 0xD and 0xE are fences.
bool IsFenceStyle(unsigned char style) { return style == 2 || style == 0xc || style == 0xd || style == 0xe; }

// Room array markers while the rooms are computed.
constexpr unsigned short kRoomSplit = 0xfffb;      // split by a diagonal wall (RoomManager's diagonals)
constexpr unsigned short kRoomQueued = 0xfffc;     // in the current room, waiting on the stack
constexpr unsigned short kRoomHalfB = 0xfffd;      // one half of a diagonal tile reached
constexpr unsigned short kRoomHalfA = 0xfffe;
constexpr unsigned short kRoomNone = 0xffff;
}  // namespace

// 8014D624
// Floods the tile grid at `level` (0: level 1) between walls into rooms: the lot's edge is room 0 and
// so is everything reached from it (the outside); every other closed area gets the next id and a Room
// with its tiles from the RoomManager. Tiles a diagonal wall splits keep each half's room in the
// RoomManager (0xFFFB in the array). Fences close rooms but join their lighting groups. Then a second
// flood from the corner marks the tiles reached without crossing a wall (flags 2 and 4: outside), and
// the inside tiles get flag 1. Finally the extent of the tiles without flag 0x10.
void cFixedWorldImpl::ComputeRooms(int level) {
  std::vector<CTilePt> stack;
  stack.reserve(static_cast<size_t>(m_xSize * m_ySize));
  std::vector<CTilePt> roomTiles;
  roomTiles.reserve(static_cast<size_t>(m_xSize * m_ySize));
  RoomManager* manager = RoomManager::GetRoomManager();
  int first = level, last = level;
  if (level == 0) first = last = 1;
  manager->ResetRooms();
  manager->ClearRoomPartitions();
  static const unsigned char kSegmentTo[4] = {8, 2, 4, 1};  // the neighbour's segment facing the tile
  for (int lv = first; lv <= last; ++lv) {
    unsigned int cur = 0;
    stack.clear();
    {
      unsigned short* room = reinterpret_cast<unsigned short*>(m_rooms->m_rows[0]);
      for (int i = m_rooms->m_xSize * m_rooms->m_ySize - 1; i >= 0; --i) *room++ = kRoomNone;
    }
    for (int x = 0; x < m_flags->m_xSize; ++x)
      for (int y = 0; y < m_flags->m_ySize; ++y) *m_flags->At(x, y) &= 0x78;
    // The lot's edge: room 0, flag 4.
    for (int x = 0; x < m_xSize; ++x) {
      *reinterpret_cast<unsigned short*>(m_rooms->At(x, 0)) = 0;
      *m_flags->At(x, 0) |= 4;
      *reinterpret_cast<unsigned short*>(m_rooms->At(x, m_ySize - 1)) = 0;
      *m_flags->At(x, m_ySize - 1) |= 4;
    }
    for (int y = 0; y < m_ySize; ++y) {
      *reinterpret_cast<unsigned short*>(m_rooms->At(0, y)) = 0;
      *m_flags->At(0, y) |= 4;
      *reinterpret_cast<unsigned short*>(m_rooms->At(m_xSize - 1, y)) = 0;
      *m_flags->At(m_xSize - 1, y) |= 4;
    }
    auto roomAt = [this](const CTilePt& t) -> unsigned short& {
      return *reinterpret_cast<unsigned short*>(m_rooms->At(t.GetX(), t.GetY()));
    };
    for (int x = 1; x < m_xSize - 1; ++x) {
      for (int y = 1; y < m_ySize - 1; ++y) {
        const unsigned short start = *reinterpret_cast<unsigned short*>(m_rooms->At(x, y));
        if (!(start > kRoomQueued || start == kRoomSplit)) continue;
        CTilePt tile(x, y, lv);
        if (start == kRoomSplit) {
          unsigned short room1, room2;
          Room__Sides side1, side2;
          manager->ResolveDiagonal(tile, &room1, &room2, &side1, &side2);
          if (room2 != kRoomNone) continue;  // both halves have their rooms
        }
        const unsigned int roomId = cur;
        roomTiles.clear();
        if (start == kRoomSplit) {
          const unsigned char* w = m_walls->At(x, y);
          if ((w[0] & 0x30) && IsFenceStyle(w[3])) {
            unsigned short room1, room2;
            Room__Sides side1, side2;
            manager->ResolveDiagonal(tile, &room1, &room2, &side1, &side2);
            manager->ChangeLightingGroup(room1, static_cast<unsigned short>(cur));
          }
        }
        stack.push_back(tile);
        while (!stack.empty()) {
          CTilePt t = stack.back();
          stack.pop_back();
          const unsigned char* wt = m_walls->At(t.GetX(), t.GetY());
          int open[4] = {1, 1, 1, 1};  // the directions (sDirections 0-3) the room spreads to
          const unsigned short r = roomAt(t);
          if (r == cur) continue;
          bool degenerate = false;
          int side = 0;
          if (r == kRoomQueued) {
            roomAt(t) = static_cast<unsigned short>(cur);
          } else if (r == kRoomNone) {
            if (wt[0] & 0x10) {
              open[3] = open[1] = 0;
              roomAt(t) = kRoomHalfA;
            } else if (wt[0] & 0x20) {
              open[2] = open[1] = 0;
              roomAt(t) = kRoomHalfB;
            } else {
              roomAt(t) = kRoomQueued;
            }
            stack.push_back(t);
            roomTiles.push_back(t);
          } else if (r == kRoomHalfA) {
            if (wt[0] & 0x10) {
              open[3] = open[1] = 0;
              side = 4;
              degenerate = true;
            } else if (wt[0] & 0x20) {
              open[0] = open[3] = 0;
              side = 1;
              degenerate = true;
            }
          } else if (r == kRoomHalfB) {
            if (!(wt[0] & 0x10)) {
              open[2] = open[1] = 0;
              side = 3;
            } else {
              open[2] = open[0] = 0;
              side = 2;
            }
            degenerate = true;
          } else if (r == kRoomSplit) {
            unsigned short room1 = 0, room2 = 0;
            Room__Sides side1, side2;
            manager->ResolveDiagonal(t, &room1, &room2, &side1, &side2);
            if (room2 == cur) continue;
            if (room2 == kRoomNone) roomTiles.push_back(t);
            side = side2;
            switch (side) {
              case 2: open[2] = open[0] = 0; break;
              case 1: open[0] = open[3] = 0; break;
              case 3: open[2] = open[1] = 0; break;
              case 4: open[1] = open[3] = 0; break;
              default: break;
            }
            degenerate = true;
          }
          if (degenerate) {
            if (manager->ProcessDegenerateTile(t, static_cast<unsigned short>(cur), Room__Sides{side}))
              roomAt(t) = kRoomSplit;
            else
              roomAt(t) = static_cast<unsigned short>(cur);
          }
          // The neighbours the room spreads to.
          for (int i = 0; i < 4; ++i) {
            if (!open[i]) continue;
            CTilePt n = t + CTilePt(TilePtDir{i}, 0);
            if (OutOfBounds(n)) continue;
            unsigned short rn = roomAt(n);
            if (rn == cur || rn == kRoomQueued) continue;
            const unsigned char* wn = m_walls->At(n.GetX(), n.GetY());
            if (wn[0] & kSegmentTo[i]) {
              // A wall between them; a fence joins the rooms' lights.
              unsigned char style = 0;
              if (i == 0) style = wt[3];
              else if (i == 1) style = wn[3];
              else if (i == 2) style = wt[2];
              else style = wn[2];
              if (IsFenceStyle(style) && rn < kRoomSplit) manager->ChangeLightingGroup(rn, static_cast<unsigned short>(cur));
              continue;
            }
            if (!(wn[0] & 0x30)) {
              rn = kRoomQueued;
            } else {
              // A diagonal in the neighbour: the half facing this tile.
              int half = 0;
              if (!(wn[0] & 0x10)) half = (i == 0 || i == 3) ? 1 : 3;
              else half = (i == 1 || i == 3) ? 4 : 2;
              if (rn == kRoomHalfB) {
                if (half != 1 && half != 4) continue;
                rn = kRoomQueued;
              } else if (rn < kRoomHalfA) {
                if (rn == kRoomSplit) {
                  unsigned short a = 0xffff, b = 0xffff;
                  Room__Sides sideA{0}, sideB{0};
                  manager->ResolveDiagonal(n, &a, &b, &sideA, &sideB);
                  unsigned short here = a, other = b;
                  if (sideA == half) {
                    here = b;
                    other = a;
                  }
                  if (here != cur && here != kRoomNone && IsFenceStyle(wn[3])) manager->ChangeLightingGroup(here, static_cast<unsigned short>(cur));
                  if (other != kRoomNone) continue;
                }
              } else if (rn == kRoomHalfA) {
                if (half != 2 && half != 3) continue;
                rn = kRoomQueued;
              } else if (rn == kRoomNone) {
                if (half == 1 || half == 4) rn = kRoomHalfA;
                else if (half == 2 || half == 3) rn = kRoomHalfB;
              }
            }
            stack.push_back(n);
            SetRoom(n, rn);
            roomTiles.push_back(n);
          }
        }
        if (!roomTiles.empty()) {
          Room* room = manager->GetNewRoom(static_cast<unsigned short>(cur));
          cur = (cur + 1) & 0xffff;
          if (!room) {
            std::fprintf(stderr, "Room manager return a NULL room...\n");  // GC: ctgDump
            continue;
          }
          room->AbsorbNewRoomList(roomTiles);
          room->m_lightingGroup = static_cast<unsigned short>(roomId);
        }
      }
    }
    manager->ChangeLightingGroup(0, 0);

    // The outside: everything reached from the corner without crossing a wall (fences do not stop
    // it, diagonal walls do).
    stack.push_back(CTilePt(1, 1, lv));
    while (!stack.empty()) {
      CTilePt t = stack.back();
      stack.pop_back();
      const int x = t.GetX(), y = t.GetY();
      unsigned char f = *m_flags->At(x, y);
      if (!(f & 2)) {
        f |= 6;
        TileWalls walls = GetWall(t);
        auto blocked = [this](int nx, int ny) {
          const unsigned char* w = m_walls->At(nx, ny);
          return (w[0] & 0x30) && !IsFenceStyle(w[3]);
        };
        if (!walls.HasDiagonalNotFence()) {
          if (x > 1 && !(*m_flags->At(x - 1, y) & 2) && !walls.HasWallNotFence(1) && !blocked(x - 1, y))
            stack.push_back(CTilePt(x - 1, y, lv));
          if (x < m_xSize - 2 && !(*m_flags->At(x + 1, y) & 2) && !walls.HasWallNotFence(4) && !blocked(x + 1, y))
            stack.push_back(CTilePt(x + 1, y, lv));
          if (y > 1 && !(*m_flags->At(x, y - 1) & 2) && !walls.HasWallNotFence(2) && !blocked(x, y - 1))
            stack.push_back(CTilePt(x, y - 1, lv));
          if (y < m_ySize - 2 && !(*m_flags->At(x, y + 1) & 2) && !walls.HasWallNotFence(8) && !blocked(x, y + 1))
            stack.push_back(CTilePt(x, y + 1, lv));
        } else {
          // A diagonal wall: the tiles on both sides of it. PC: the decompilation's condition-register
          // logic is unclear here; each unvisited neighbour is taken (GC's tests read the flags of
          // (x, y -/+ 1) by the diagonal's direction and range-check both y neighbours on the first).
          const bool d10 = walls.HasWall(0x10);
          const int yA = d10 ? y + 1 : y - 1;
          const int yB = d10 ? y - 1 : y + 1;
          const bool yIn = yA >= 1 && yA <= m_ySize - 2;
          const bool xIn = x >= 1 && x <= m_xSize - 2;
          if (xIn && !(*m_flags->At(x + 1, y) & 2)) stack.push_back(CTilePt(x + 1, y, lv));
          if (yIn && !(*m_flags->At(x, yA) & 2)) stack.push_back(CTilePt(x, yA, lv));
          if (yIn && !(*m_flags->At(x, yB) & 2)) stack.push_back(CTilePt(x, yB, lv));
          if (xIn && !(*m_flags->At(x - 1, y) & 2)) stack.push_back(CTilePt(x - 1, y, lv));
        }
      }
      *m_flags->At(x, y) = f;
    }
    for (int x = 1; x < m_xSize - 1; ++x)
      for (int y = 1; y < m_ySize - 1; ++y) *m_flags->At(x, y) |= 1;
  }
  // GC: GlobalDispatch(0xEC, level): HouseImpl::DoCommand tells the object module (vtable 0x7C) and
  // recomputes the rooms' statistics.
  HouseDispatch(0xec, level);
  if (!Globs_pHouse) {  // PC: no house yet (the house object is made with the first lot)
    if (Globs_pObjectModule) Globs_pObjectModule->UpdateRooms(level);
    manager->ComputeRooms();
  }
  m_maxY = -1;
  m_minX = m_xSize;
  m_minY = m_ySize;
  m_maxX = -1;
  for (int x = 0; x < m_xSize; ++x)
    for (int y = 0; y < m_ySize; ++y) {
      CTilePt t(x, y, 1);
      if (GetFlags(t) & 0x10) continue;
      if (y < m_minY) m_minY = y;
      if (m_maxY < y) m_maxY = y;
      if (x < m_minX) m_minX = x;
      if (m_maxX < x) m_maxX = x;
    }
}

// ---------------------------------------------------------------------------------------------------
// The house's architecture value (game/world.cpp)

namespace {
// GetWallPrice (8014C444): a fence style (2, 0xC, 0xD, 0xE) costs its FenceSet row's price less a fifth;
// any other wall 1.
int WallPriceForStyle(int style) {
  if (style != 2 && style != 0xc && style != 0xd && style != 0xe) return 1;
  const void* set = _globals.m_fenceSet;  // EGlobal +0xDC
  const unsigned char* rows = set ? static_cast<const unsigned char*>(ERQuickdata::Ptr(set, 0)) : nullptr;
  const int n = rows ? static_cast<int>(ERQuickdata::U32(rows - 4)) : 0;
  for (int i = 0; i < n; ++i) {
    const void* row = ERQuickdata::Ptr(rows, static_cast<unsigned int>(i) * 4);
    if (row && ERQuickdata::S32(row, 0xc) == style) return static_cast<int>(static_cast<float>(ERQuickdata::U32(row, 4)) * 0.8f);
  }
  return 0;
}
// A build set's row `index` price (its first word): "FloorSet" (+0xD4) or "WallSet" (+0xD8).
unsigned int SetPrice(const void* set, int index) {
  const unsigned char* rows = set ? static_cast<const unsigned char*>(ERQuickdata::Ptr(set, 0)) : nullptr;
  const int n = rows ? static_cast<int>(ERQuickdata::U32(rows - 4)) : 0;
  if (index < 0 || index >= n) return 0;
  const void* row = ERQuickdata::Ptr(rows, static_cast<unsigned int>(index) * 4);
  return row ? ERQuickdata::U32(row, 0) : 0;
}
}  // namespace

// 80150414
// Every tile of the lot's first level (but those flagged 0x20): its walls at their prices (fences at four
// fifths of the catalogue's, halved in the total) and the wallpaper on plain walls (styles 1, 7, 8, 9);
// its floor (a diagonal tile's two halves at half price each). *built: a wall or floor anywhere.
RET_UNKNOWN cFixedWorldImpl::ComputeArchValue(bool* built) {
  *built = false;
  int value = 0, walls = 0;
  for (int y = 0; y < m_ySize; ++y)
    for (int x = 0; x < m_xSize; ++x) {
      CTilePt tile(x, y, 1);
      if (GetFlags(tile) & 0x20) continue;
      TileWalls tw = GetWall(tile);
      *built = *built || tw.HasWall();
      for (int seg = tw.First(); seg != 0; seg = tw.Next(TileWallsSegment{seg})) {
        const int style = tw.GetStyle(TileWallsSegment{seg});
        walls += WallPriceForStyle(style);
        if (style == 1 || style == 7 || style == 8 || style == 9)
          value += static_cast<int>(SetPrice(_globals.m_wallSet, tw.GetPattern(TileWallsSegment{seg}, DiagonalSideSelector{0})));
      }
      int floor = GetFloor(tile);
      if (floor != 0) {
        unsigned int price = 0;
        if (floor == 0xff) {  // a diagonal tile: each half
          int other = 0;
          if (tw.HasWall(TileWallsSegment{0x10})) {
            if (int f = tw.GetFloorValue(DiagonalSideSelector{2})) value += static_cast<int>(SetPrice(_globals.m_floorSet, f) >> 1);
            other = 4;
          } else if (tw.HasWall(TileWallsSegment{0x20})) {
            if (int f = tw.GetFloorValue(DiagonalSideSelector{1})) value += static_cast<int>(SetPrice(_globals.m_floorSet, f) >> 1);
            other = 3;
          }
          const int f = other ? tw.GetFloorValue(DiagonalSideSelector{other}) : 0;
          if (f) price = SetPrice(_globals.m_floorSet, f) >> 1;
        } else {
          price = SetPrice(_globals.m_floorSet, floor);
        }
        value += static_cast<int>(price);
      }
      *built = *built || floor != 0;
    }
  return value + walls / 2;
}
