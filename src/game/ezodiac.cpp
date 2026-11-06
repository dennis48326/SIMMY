// Port of game/ezodiac.cpp (part): the star sign whose personality points are nearest to a sim's.
// Ported too: the localised sign names (InitZodiac, GetZodiacName). Not ported: the compatibilities.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
// zodiacPersonalityValues (ezodiac.cpp's copy): the points of the twelve signs, Aries first, in the
// description's order (outgoing, nice, playful, neat, active).
const signed char kZodiacPersonalityValues[12][5] = {
    {8, 3, 3, 5, 6}, {5, 4, 8, 5, 3}, {7, 3, 3, 4, 8}, {3, 6, 4, 6, 6}, {10, 3, 4, 4, 4}, {2, 5, 3, 9, 6},
    {8, 7, 6, 2, 2}, {5, 3, 3, 6, 8}, {3, 4, 7, 2, 9}, {4, 5, 8, 7, 1}, {4, 6, 7, 4, 4}, {3, 7, 3, 5, 7},
};
// sLocalized (80475B54): the signs' names, Aries first; sCompatibleWith / sIncompatibleWith.
const unsigned short* const* sLocalized[12] = {};
const unsigned short* const* sCompatibleWith = nullptr;
const unsigned short* const* sIncompatibleWith = nullptr;
int sInited = 0;
}  // namespace

// 8003959C
// The signs' names (PDA strings "zodiac_aries" ...) and the compatibility words, once.
int InitZodiac() {
  if (!sInited) {
    static const char* const kNames[12] = {"zodiac_aries",  "zodiac_taurus",  "zodiac_gemini",      "zodiac_cancer",
                                           "zodiac_leo",    "zodiac_virgo",   "zodiac_libra",       "zodiac_scorpio",
                                           "zodiac_sagittarius", "zodiac_capricorn", "zodiac_aquarius", "zodiac_pisces"};
    for (int i = 0; i < 12; ++i) sLocalized[i] = _globals.GetPDAString(kNames[i]);
    sCompatibleWith = _globals.GetNewUIString("compatiblewith");
    sIncompatibleWith = _globals.GetNewUIString("incompatiblewith");
    sInited = 1;
  }
  return sInited;
}

// 80039C38
// Sign 1..12's name (out of range: the nearest).
const unsigned short* GetZodiacName(short sign) {
  if (!sInited) InitZodiac();
  if (sign < 1) sign = 1;
  else if (sign > 12) sign = 12;
  const unsigned short* const* slot = sLocalized[sign - 1];
  return slot ? *slot : nullptr;
}

// 800397A4
// The sign (1 Aries .. 12 Pisces) with the least squared distance to the five points.
short ComputeZodiacSignAux(float* points) {
  float best = 7736.0f;
  int sign = -1;
  for (int s = 1; s < 13; ++s) {
    float d = 0.0f;
    for (int i = 0; i < 5; ++i) {
      const float x = points[i] - static_cast<float>(kZodiacPersonalityValues[s - 1][i]);
      d = x * x + d;
    }
    if (d < best) {
      sign = s;
      best = d;
    }
  }
  if (static_cast<unsigned int>(sign - 1) > 0xb) sign = 1;
  return static_cast<short>(sign);
}

// 80039868
// The sign of the person data's personality (x100: outgoing, nice, playful, neat, active).
short ComputeZodiacSign(short* pers) {
  static const int kLocalToPersonDataMap[5] = {6, 2, 5, 7, 3};  // sLocalToPersonDataMap (80423DFC)
  float points[5];
  for (int i = 0; i < 5; ++i) points[i] = static_cast<float>(pers[kLocalToPersonDataMap[i]]) * 0.01f;
  return ComputeZodiacSignAux(points);
}

// 800398EC
short ComputeZodiacSignFromSimDesc(signed char* personality) {
  float points[5];
  for (int i = 0; i < 5; ++i) points[i] = static_cast<float>(personality[i]);
  return ComputeZodiacSignAux(points);
}
