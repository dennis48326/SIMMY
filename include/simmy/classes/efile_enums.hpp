// @class EFile__DeviceType
// @class EFile__AccessMode
// @class EFile__IOMode
// @class EFile__SeekType
// EFile enums (base/e_file.cpp). 0x80000000 means "default / don't care" throughout.
#pragma once
#include "simmy/enum.hpp"

struct EFile__DeviceType {
  SIMMY_ENUM_BODY(EFile__DeviceType)
  static constexpr int Default = static_cast<int>(0x80000000u);
  static constexpr int SNHost = 2;  // SN Systems host file server (debug PC files)
  static constexpr int Disc = 3;    // the game disc
};
struct EFile__AccessMode {
  SIMMY_ENUM_BODY(EFile__AccessMode)
  static constexpr int Default = static_cast<int>(0x80000000u);
};
struct EFile__IOMode {  // bit set built by EFileSystem::ParseMode
  SIMMY_ENUM_BODY(EFile__IOMode)
  static constexpr int Read = 0x1;
  static constexpr int Write = 0x2;
  static constexpr int Append = 0x4;
  static constexpr int Create = 0x10000;
};
struct EFile__SeekType {
  SIMMY_ENUM_BODY(EFile__SeekType)
  static constexpr int Set = 0;
  static constexpr int Cur = 1;
  static constexpr int End = 2;
};
