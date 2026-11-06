// @class CTilePt
// @class TilePtDir
// A tile position (game/ctilept.cpp): x, y (signed bytes) and the level (floor).
#pragma once

// An index into CTilePt::sDirections.
struct TilePtDir {
  SIMMY_ENUM_BODY(TilePtDir)
};

struct CTilePt {
  CTilePt(EVec3 &);  // 800B43EC
  void GetEVec3() const;  // 800B4430
  void GetEVec3M() const;  // 800B44C8
  double GetXf() const;  // 800B4560
  double GetYf() const;  // 800B4598
  CTilePt();  // 800B45D0
  CTilePt(const CTilePt &);  // 800B45DC
  CTilePt(FTilePt &, int);  // 800B45FC
  CTilePt(TilePt &, int);  // 800B4620
  CTilePt(int x, int y, int level);  // 800B463C
  CTilePt(float, float, int);  // 800B4678
  CTilePt(TilePtDir dir, int level);  // 800B46D8
  ~CTilePt();  // 800B4724
  CTilePt& operator=(const CTilePt &);  // 800B474C
  bool operator==(const CTilePt &) const;  // 800B476C
  bool operator!=(const CTilePt &) const;  // 800B479C
  void operator+=(const CTilePt &);  // 800B4810
  void operator-=(const CTilePt &);  // 800B4838
  void operator*=(int);  // 800B4860
  RET_UNKNOWN operator*(int) const;  // 800B4888
  CTilePt operator+(const CTilePt &) const;  // 800B48D8
  RET_UNKNOWN operator-(CTilePt &) const;  // 800B4938
  int GetRow() const;  // 800B49EC
  int GetColumn() const;  // 800B4A04
  bool IsoCompare(CTilePt &) const;  // 800B4A1C
  bool IsNorthOf(CTilePt &) const;  // 800B4A98
  bool IsSouthOf(CTilePt &) const;  // 800B4ADC
  bool IsWestOf(CTilePt &) const;  // 800B4B20
  bool IsEastOf(CTilePt &) const;  // 800B4B64
  bool SameRowParity(CTilePt &) const;  // 800B4BA8
  bool SameColumnParity(CTilePt &) const;  // 800B4BF4
  void ToTilePt() const;  // 800B4C40
  void ToFTilePt() const;  // 800B4C60
  int GetX() const;  // 800B4CA8
  int GetY() const;  // 800B4CB4
  void Get(int *, int *) const;  // 800B4CC0
  void Get(int *, int *, int *) const;  // 800B4CDC
  RET_UNKNOWN SetX(int);  // 800B4D04
  RET_UNKNOWN SetY(int);  // 800B4D10
  void Set(int, int);  // 800B4D1C
  void Set(int x, int y, int level);  // 800B4D28
  RET_UNKNOWN SetX(float);  // 800B4D38
  RET_UNKNOWN SetY(float);  // 800B4D58
  void Set(float, float);  // 800B4D78
  void Set(float, float, int);  // 800B4DA8
  int GetLevel() const;  // 800B4DDC
  void SetLevel(int);  // 800B4DE8
  void SetLevel(CTilePt &);  // 800B4DF0
  static RET_UNKNOWN GetDirection(CTilePt &, CTilePt &);  // 800B4E00
  static int GetTileDirection(CTilePt& from, CTilePt& to);  // 800B4ED8

  static CTilePt sDirections[8];  // 80485E20 by TilePtDir: (0,-1) (0,1) (-1,0) (1,0) (-1,-1) (1,1) (1,-1) (-1,1)

  signed char m_x = 0;      // +0x00
  signed char m_y = 0;      // +0x01
  signed char m_level = 0;  // +0x02
};
