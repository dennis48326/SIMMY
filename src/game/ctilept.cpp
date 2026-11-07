// Port of game/ctilept.cpp (partial): tile positions.
#include "simmy/types_gen.hpp"

// 800B5194 (global constructors: __static_initialization_and_destruction_0, 800B505C)
CTilePt CTilePt::sDirections[8] = {CTilePt(0, -1, 0), CTilePt(0, 1, 0),   CTilePt(-1, 0, 0), CTilePt(1, 0, 0),
                                   CTilePt(-1, -1, 0), CTilePt(1, 1, 0), CTilePt(1, -1, 0), CTilePt(-1, 1, 0)};

// 800B45D0
CTilePt::CTilePt() {}

// 800B45DC
CTilePt::CTilePt(const CTilePt& other) : m_x(other.m_x), m_y(other.m_y), m_level(other.m_level) {}

// 800B463C
// Coordinates that do not fit a signed byte become 0.
// 800B43EC
// The tile of a point (x, y truncated), on level 1.
CTilePt::CTilePt(EVec3& v) {
  m_x = static_cast<signed char>(static_cast<int>(v.x));
  m_y = static_cast<signed char>(static_cast<int>(v.y));
  m_level = 1;
}

CTilePt::CTilePt(int x, int y, int level) {
  m_x = static_cast<signed char>(x);
  m_y = static_cast<signed char>(y);
  if (m_x != x) m_x = 0;
  if (m_y != y) m_y = 0;
  m_level = static_cast<signed char>(level);
}

// 800B46D8
CTilePt::CTilePt(TilePtDir dir, int level) {
  *this = sDirections[dir];
  m_level = static_cast<signed char>(level);
}

// 800B4724
CTilePt::~CTilePt() {}

// 800B474C
CTilePt& CTilePt::operator=(const CTilePt& other) {
  m_x = other.m_x;
  m_y = other.m_y;
  m_level = other.m_level;
  return *this;
}

// 800B476C
bool CTilePt::operator==(const CTilePt& other) const {
  return m_x == other.m_x && m_y == other.m_y;  // the level is not compared
}

// 800B48D8
// The sum of the coordinates, at this level.
CTilePt CTilePt::operator+(const CTilePt& other) const { return CTilePt(m_x + other.m_x, m_y + other.m_y, m_level); }

// 800B4CA8
int CTilePt::GetX() const { return m_x; }
// 800B4CB4
int CTilePt::GetY() const { return m_y; }

// 800B4D1C
void CTilePt::Set(int x, int y) {
  m_y = static_cast<signed char>(y);
  m_x = static_cast<signed char>(x);
}

// 800B4D28
void CTilePt::Set(int x, int y, int level) {
  m_level = static_cast<signed char>(level);
  m_x = static_cast<signed char>(x);
  m_y = static_cast<signed char>(y);
}

// 800B4DDC
int CTilePt::GetLevel() const { return m_level; }

// 800B479C
bool CTilePt::operator!=(const CTilePt& other) const { return other.m_x != m_x || other.m_y != m_y; }
// 800B4810
void CTilePt::operator+=(const CTilePt& d) {
  m_x = static_cast<signed char>(m_x + d.m_x);
  m_y = static_cast<signed char>(m_y + d.m_y);
}
// 800B4838
void CTilePt::operator-=(const CTilePt& d) {
  m_x = static_cast<signed char>(m_x - d.m_x);
  m_y = static_cast<signed char>(m_y - d.m_y);
}
// 800B49EC
int CTilePt::GetRow() const { return m_x + m_y; }
// 800B4A04
int CTilePt::GetColumn() const { return m_x - m_y; }
// 800B4DA8
void CTilePt::Set(float x, float y, int level) {
  m_level = static_cast<signed char>(level);
  m_x = static_cast<signed char>(static_cast<int>(x));
  m_y = static_cast<signed char>(static_cast<int>(y));
}

// 800B4ED8
// The direction (TilePtDir) from `from` towards `to` along the straightest of the four lines: 1 / 0
// along y (down / up), 3 / 2 along x, 5 / 4 along a row (x + y), 6 / 7 along a column (x - y); 8 the
// same tile.
int CTilePt::GetTileDirection(CTilePt& from, CTilePt& to) {
  if (from == to) return 8;
  const int dx = to.GetX() - from.GetX(), dy = to.GetY() - from.GetY();
  const int row2 = to.GetRow(), col2 = to.GetColumn(), row1 = from.GetRow(), col1 = from.GetColumn();
  const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  const int dc = col2 - col1, dr = row2 - row1;
  const int ac = (dc < 0 ? -dc : dc) / 2, ar = (dr < 0 ? -dr : dr) / 2;
  if (ax < ay && ax <= ac && ax <= ar) return dy >= 0 ? 1 : 0;
  if (ay < ax && ay <= ac && ay <= ar) return dx < 0 ? 2 : 3;
  if (ac < ax && ac < ay && ac < ar) return row2 < row1 ? 4 : 5;
  return col2 < col1 ? 7 : 6;
}
