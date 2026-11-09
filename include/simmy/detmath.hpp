// Deterministic float math for the game (PC: no GameCube counterpart as a header).
//
// The GameCube game called the SN Systems libm (newlib's single-precision fdlibm, compiled with fused
// multiply-adds): sinf, cosf, tanf, atanf, atan2f, asinf, acosf, expf, logf, log10f, powf. Those are
// ported in src/platform/detmath/detmath.cpp, instruction for instruction (every fmadds/fmsubs is a
// correctly rounded fused multiply-add done in portable code), so the results are the GameCube's and are
// bit-identical with every compiler, CRT and CPU: MSVC's UCRT, glibc and the CPU's FMA paths are never
// used. Game code (src/base, src/engine, src/game, src/libaptz) must call these, never std::sin & co.,
// whose last bits differ between Windows and Linux (and can differ between CPUs): a lockstep desync.
//
// Only the float versions exist (the GameCube had no others): a double argument does not compile, so
// say static_cast<float>(...) where the port computes in double (the GameCube converted with frsp).
// Exact operations stay on the C library: sqrt, floor, ceil, round, trunc, fmod, fabs (IEEE-exact,
// identical everywhere).
#pragma once

namespace simmy::dm {

float sin(float x);              // 80241464 sinf
float cos(float x);              // 8024129C cosf
float tan(float x);              // 80241540 tanf
float atan(float x);             // 80240FC8 atanf
float atan2(float y, float x);   // 80240AE8 atan2f
float asin(float x);             // 8036D2A8 asinf
float acos(float x);             // 8024089C acosf
float exp(float x);              // 8036D4D4 expf
float log(float x);              // 8036D700 logf
float log10(float x);            // 8036D980 log10f
float pow(float x, float y);     // 8036DA8C powf
float fmadds(float a, float c, float b);  // a * c + b rounded once (the PowerPC fmadds), portable

// Doubles (and ints) are not converted implicitly: write static_cast<float>(...).
template <class T> float sin(T) = delete;
template <class T> float cos(T) = delete;
template <class T> float tan(T) = delete;
template <class T> float atan(T) = delete;
template <class T, class U> float atan2(T, U) = delete;
template <class T> float asin(T) = delete;
template <class T> float acos(T) = delete;
template <class T> float exp(T) = delete;
template <class T> float log(T) = delete;
template <class T> float log10(T) = delete;
template <class T, class U> float pow(T, U) = delete;

// `simmy --mathcheck [file]`: runs fixed inputs through every function above and prints one hash per
// function and a total (docs/determinism-and-netplay.md lists the expected values); the same numbers
// on every build mean the math is identical. Also prints the C library's results' hash for comparison
// (that one is expected to differ between Windows and Linux).
int mathcheck(const char* outPath);
// PC mod: network play: the same total as a number (the handshake compares it).
unsigned long long mathcheck_hash();

}  // namespace simmy::dm
