// Common definitions for the SIMMY PC port.
#pragma once
#include <cstring>
#include <type_traits>
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <ctime>

#include <dolphin/types.h>

// Return type not yet recovered for this function (GCC 2.95 mangling has no return types).
// Pointer-sized so it can hold either an int or a pointer until the real type is known.
using RET_UNKNOWN = std::intptr_t;

namespace simmy {
// Logs the first call of each unported function (see port/src/platform/stub.cpp).
void stub_hit(const char* name, std::uint32_t addr);
} // namespace simmy

#define SIMMY_STUB(name, addr) ::simmy::stub_hit(name, addr)

namespace simmy {
// The GC libc rand (802432B0) in two streams (platform/pc_det.cpp): the simulation's and the UI's.
int sim_rand();
unsigned int sim_rand_state();
void set_sim_rand_state(unsigned int state);
int ui_rand();
unsigned int ui_rand_state();
unsigned int psystem_rand_state();  // engine/psystem.cpp (the particle system's)
// The game's clock in nanoseconds: the wall clock, or with SIMMY_DET a virtual clock of one 60 Hz
// retrace per game frame (platform/pc_det.cpp).
unsigned long long game_clock_ns();
} // namespace simmy

// GetNextRandomNumber's generator with its own state, for the sound code (game/cassimstate.cpp).
unsigned int simmy_sound_rand();

namespace simmy {
// Game data is big-endian (GameCube). Read it through these instead of dereferencing.
inline std::uint32_t be32(const void* p) {
  const auto* b = static_cast<const unsigned char*>(p);
  return (std::uint32_t(b[0]) << 24) | (std::uint32_t(b[1]) << 16) | (std::uint32_t(b[2]) << 8) | b[3];
}
inline std::uint16_t be16(const void* p) {
  const auto* b = static_cast<const unsigned char*>(p);
  return static_cast<std::uint16_t>((b[0] << 8) | b[1]);
}
inline float befloat(const void* p) {
  const std::uint32_t u = be32(p);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

// Big-endian field readers for Load(EFile&) / Read(EStream&) ports. The original read raw bytes into
// its fields (the GameCube is big-endian); on PC every field read goes through one of these.
template <class F> std::uint32_t read_u32(F& f) {
  unsigned char b[4] = {};
  f.Read(b, 4);
  return be32(b);
}
template <class F> std::uint16_t read_u16(F& f) {
  unsigned char b[2] = {};
  f.Read(b, 2);
  return be16(b);
}
template <class F> std::uint8_t read_u8(F& f) {
  unsigned char b = 0;
  f.Read(&b, 1);
  return b;
}
template <class F> float read_float(F& f) {
  unsigned char b[4] = {};
  f.Read(b, 4);
  return befloat(b);
}
template <class F, class T> void read_be(F& f, T& v) {
  if constexpr (sizeof(T) == 1) v = static_cast<T>(read_u8(f));
  else if constexpr (sizeof(T) == 2) v = static_cast<T>(read_u16(f));
  else if constexpr (std::is_floating_point_v<T>) v = read_float(f);
  else v = static_cast<T>(read_u32(f));
}
}  // namespace simmy

// PC mod: 4 players (docs/multiplayer.md). The GameCube game has two players (kGcPlayers); the PC
// port's free play can take up to kMaxPlayers (players= in simmy.ini). Per-player arrays that the
// extra players use are sized kMaxPlayers; slots 0 and 1 keep the GC layout and code paths.
constexpr int kGcPlayers = 2;
constexpr int kMaxPlayers = 4;

// SDK types that game signatures refer to.
#include <dolphin/dolphin.h>
#include <dolphin/thp.h>
