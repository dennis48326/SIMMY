// @class ReconBuffer after TileWalls CTilePt Room
// @class ReconBuffer__Mode
// @class Scheme
// @class ReconObject
// @class ReconBuilder
// @class _c2DArray
// @class cFixedWorldImpl
// The lot's tile grid (game/world.cpp, Globs::pFixedWorld): per tile its floor pattern, flags, room
// and walls, as 2D arrays (game/newarray2d.cpp) read from the house's 'Arry' resources, which are
// streamed (run-length) through a ReconBuffer (game/reconbuffer.cpp).
#pragma once
#include <string>

struct ReconBuffer__Mode {
  SIMMY_ENUM_BODY(ReconBuffer__Mode)
  enum : int { kRead = 0, kWrite = 1, kMeasure = 2 };
};

// A compressed integer coding (game/reconbuffer.cpp scheme8/16/32): a value takes a 1 bit "non-zero"
// flag, then a 2 bit choice of the four widths and the value sign-extended from that width (`mask` is
// the bits above the width's sign bit).
struct Scheme {
  struct Width {
    int bits;
    unsigned int mask;
  };
  Width widths[4];
};

// A read or write cursor over a buffer (0x30 bytes). Mode 0 reads, 1 writes, 2 measures.
struct ReconBuffer {
  using Mode = ::ReconBuffer__Mode;
  ReconBuffer(void* data, int size, ReconBuffer__Mode mode, bool compressed);  // 80132D9C
  ReconBuffer();  // 80132DD8
  ~ReconBuffer();  // 80132E18
  void UseStringTable(unsigned int, unsigned int, short);  // 80132E94
  void EnableCompression();  // 80132F40
  void ReconCmprInt(int* v, const Scheme* scheme);  // 80132F4C
  void ReconBits(int bits, int* v);  // 80133108
  void PadBits();  // 801332C4
  void Recon8(signed char* v, int count);  // 801332D0
  void ReconBool(int* v);  // 80133430 (GC: a bool held in an int)
  void Recon16(short* v, int count);  // 80133484
  void ReconInt(int* v, int count);  // 80133614
  void Recon32(int* v, int count);  // 80133634
  void ReconFloat(float* v, int count);  // 801337E0
  void ReconMark();  // 8013398C
  void ReadToNextMark();  // 80133A98
  // PC: a BString is a std::string, a BString2 a std::u16string (no capacity).
  void ReconString(std::string& s);  // 80133AE4 ReconString(BString&)
  // UTF-16: written 0xFFFE then big-endian characters; read back as written, as little-endian after
  // 0xFEFF, or as a narrow string.
  void ReconString(std::u16string& s);  // 80133C48 ReconString(BString2&)
  // PC: a StringBuffer(2) is its buffer and capacity (StringBuffer(char*, unsigned int)), as on GC;
  // the text read is cut to the capacity (and the cursor moved by the cut length, as on GC).
  void ReconString(char* buffer, int capacity);  // 80133ED0 ReconString(StringBuffer&)
  void ReconString(unsigned short* buffer, int capacity);  // 80134028 ReconString(StringBuffer2&)

  unsigned char* m_data = nullptr;  // +0x00
  int m_size = 0;                   // +0x04
  int m_pos = 0;                    // +0x08
  int m_mode = 0;                   // +0x0C
  int m_littleEndian = 1;           // +0x10 the buffer's words are little-endian
  int m_field14 = 0;
  int m_compressed = 0;             // +0x18 values are ReconCmprInt bit codes
  int m_field1C = 0, m_field20 = 0, m_field24 = 0;  // +0x1C string table (UseStringTable, not ported)
  int m_bitMask = 0;                // +0x28 the next bit in the byte before m_pos (0: start a new byte)
  int m_mark = 0;                   // +0x2C the next mark's position (ReconMark, ReadToNextMark)
};

// A streamed object (game/recon.cpp): its DoStream reads or writes it; GetType names its resource
// type.
struct ReconObject {
  virtual ~ReconObject();  // 80134734
  virtual void DoStream(ReconBuffer* buffer, int version);  // 80134768
  virtual int GetType();  // 8013476C
};

// Reads a resource into a ReconObject: a 12-byte header (+0 compressed flag byte, +4 version) then
// the stream (little-endian in the saves).
struct ReconBuilder {
  // The object streamed into a new resource: the 12-byte header (flags 0, version, type;
  // little-endian) then the stream.
  Memory__HandleNode* Compact(ReconObject* object, int version);  // 8013426C
  // ... added to `file` as resource `id` of the object's type: 0, else the file's error (-1: none made).
  int Compact(ReconObject* object, int version, iResFile* file, short id);  // 801343D4
  int Reconstitute(ReconObject* object, iResFile* file, short id, int* version);  // 80134518
  void Reconstitute(ReconObject* object, Memory__HandleNode* handle, int* version);  // 801345C0
  RET_UNKNOWN Swizzle(void *, int);  // 801346BC
  void SwapTwoByteNumber(unsigned short *);  // 801346FC
  void SwapFourByteNumber(unsigned int *);  // 80134710
};

// ReconLoadObject<T> (linkonce): `object`'s DoStream run over resource `id` of `type` in `file`.
template <class T>
int ReconLoadObject(T* object, iResFile* file, int type, short id, int* version) {
  struct Adapter : ReconObject {
    T* m_object;
    int m_type;
    Adapter(T* o, int t) : m_object(o), m_type(t) {}
    void DoStream(ReconBuffer* b, int v) override { m_object->DoStream(b, v); }
    int GetType() override { return m_type; }
  } adapter(object, type);
  ReconBuilder builder;
  return builder.Reconstitute(&adapter, file, id, version);
}

// ReconSaveObject<T> (linkonce): `object`'s DoStream written as resource `id` of `type` in `file`.
template <class T>
int ReconSaveObject(T* object, iResFile* file, int type, short id, int version) {
  struct Adapter : ReconObject {
    T* m_object;
    int m_type;
    Adapter(T* o, int t) : m_object(o), m_type(t) {}
    void DoStream(ReconBuffer* b, int v) override { m_object->DoStream(b, v); }
    int GetType() override { return m_type; }
  } adapter(object, type);
  ReconBuilder builder;
  return builder.Compact(&adapter, version, file, id);
}

// A 2D array of fixed-size elements (0x18 bytes): row pointers then the rows, one allocation.
struct _c2DArray {
  int ReadFromDisk(iResFile* file, int type, short id, void (*swizzle)(void*, int));  // 800D1A04
  RET_UNKNOWN Swizzle(void *, int);  // 800D1B18
  _c2DArray(int elemSize, int xSize, int ySize, BString& name);  // 800D1B1C
  ~_c2DArray();  // 800D1BA8
  static void AddArray(_c2DArray *);  // 800D1C28
  static void RemoveArray(_c2DArray *);  // 800D1C38
  int WriteToDisk(iResFile* file, int type, short id, bool compress);  // 800D1C74
  int ClearBytes(signed char value);  // 800D1E78
  int SetSize(int xSize, int ySize);  // 800D1EC4
  void DoStream(ReconBuffer* buffer, bool compressed);  // 800D1FF8
  void CopyFrom(BString &);  // 800D22E4
  void CopyFrom(_c2DArray *);  // 800D2320
  void CopyTo(_c2DArray *);  // 800D2394
  void CopyTo(BString &);  // 800D23C0
  static RET_UNKNOWN GetArray(BString &);  // 800D23FC
  void SetName(BString &);  // 800D245C

  // The element at (x, y): row x (the arrays are indexed [x][y] as the game does).
  unsigned char* At(int x, int y) const { return m_rows[x] + y * m_elemSize; }

  int m_ySize = -1;                 // +0x04 elements per row
  int m_xSize = -1;                 // +0x08 rows
  unsigned char** m_rows = nullptr; // +0x0C
  int m_elemSize = 1;               // +0x14
};

struct cFixedWorldImpl;
extern cFixedWorldImpl* Globs_pFixedWorld;  // 804FDBF8 Globs::pFixedWorld

// The world (0x44 bytes).
struct cFixedWorldImpl {
  cFixedWorldImpl(int xSize, int ySize);  // 8014C524
  virtual ~cFixedWorldImpl();  // 8014C5C0
  virtual int SetSize(int xSize, int ySize, bool keep);  // 8014C630
  virtual void DeleteArrays();  // 8014CAC0
  virtual int Save(iResFile* file, int version);  // 8014CB7C
  virtual int Load(iResFile* file, int version);  // 8014CC0C
  virtual _c2DArray* GetFloorLayer();  // 8014CDC4
  virtual _c2DArray* GetWalls();  // 8014CDCC
  virtual TileWalls GetWall(CTilePt& tile);  // 8014CDD4
  virtual void SetWall(CTilePt& tile, TileWalls walls);  // 8014D0C0
  virtual void ComputeRooms(int level);  // 8014D624
  virtual RET_UNKNOWN ComputeArchValue(bool *);  // 80150414
  virtual RET_UNKNOWN DoCommand(short, int);  // 80150758
  void OffsetWorld(CTilePt &);  // 801507F4
  virtual RET_UNKNOWN OutOfRoutableExtent(CTilePt &) const;  // 80151984
  virtual RET_UNKNOWN OutOfRoutableExtent(FTilePt &) const;  // 801519D4
  virtual int OutOfBounds(CTilePt& tile) const;  // 80151A40
  virtual int OutOfBuildingBounds(CTilePt& tile, bool inner) const;  // 80151A90
  virtual int OutOfGrid(CTilePt& tile) const;  // 80151B10
  virtual RET_UNKNOWN OutOfBounds(FTilePt &) const;  // 80151B58
  virtual RET_UNKNOWN OutOfGrid(FTilePt &) const;  // 80151BC4
  virtual int GetXSize() const;  // 80151C30
  virtual int GetYSize() const;  // 80151C38
  virtual int GetMaxSize() const;  // 80151C40
  virtual int GetFloor(CTilePt& tile);  // 80151C48
  virtual RET_UNKNOWN IsGrassAt(CTilePt &);  // 80151C98
  virtual int SetFloor(CTilePt& tile, int pattern);  // 80151D18
  virtual unsigned short GetRoom(CTilePt& tile);  // 80151DA4
  virtual void SetRoom(CTilePt& tile, unsigned short room);  // 80151E60
  virtual unsigned char GetFlags(CTilePt& tile);  // 80151EB8
  virtual void SetFlags(CTilePt& tile, unsigned char flags);  // 80151F08
  virtual int IsOutside(CTilePt& tile);  // 80151F5C
  virtual bool HasWalls(CTilePt &, TileWallsSegment);  // 80151FB0
  virtual bool HasWalls(CTilePt &);  // 80152018
  virtual RET_UNKNOWN GetWallStorage(CTilePt &);  // 80152080
  virtual void SetWallStorage(CTilePt& tile, TileWallStorage& storage);  // 801520D4
  virtual int MayEditTile(CTilePt& tile) const;  // 8015213C
  virtual unsigned short GetRoomIdFromPoint(CTilePt& tile);  // 801521AC
  virtual RET_UNKNOWN GetInfluenceMap();  // 8015235C
  virtual PassiveInfluenceMap* GetPassiveInfluenceMap();  // 80152364

  int m_xSize = 0;                  // +0x14
  int m_ySize = 0;                  // +0x18
  int m_minX = 0;                   // +0x1C the extent of the tiles without flag 0x10 (ComputeRooms)
  int m_maxX = -1;                  // +0x20
  int m_minY = 0;                   // +0x24
  int m_maxY = -1;                  // +0x28
  _c2DArray* m_floors = nullptr;    // +0x2C 'Arry' 11: floor pattern per tile
  _c2DArray* m_rooms = nullptr;     // +0x30 room id per tile (2 bytes; computed)
  _c2DArray* m_flags = nullptr;     // +0x34 'Arry' 8: tile flags (ComputeRooms: 1 inside the edge,
                                    //   2 visited, 4 outside; 0x10 off the lot's extent)
  _c2DArray* m_walls = nullptr;     // +0x38 'Arry' 12: TileWallStorage (8 bytes) per tile
  void* m_influence = nullptr;      // +0x3C InfluenceMap
  PassiveInfluenceMap* m_passiveInfluence = nullptr;  // +0x40
};
