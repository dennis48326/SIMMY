// @class levelraininfo
// @class RainEffect after levelraininfo RainEffect__RainType Emitter RParticle PVector4
// The weather (game/weather.cpp, game/eorhouse.cpp's EHouse::UpdateWeather): each house rains at a
// time of day of its own (LevelRainInfo). A RainEffect is the rain while it lasts: rain sprites
// following the camera (in the weather particle system, or the global one while the camera is
// outside) and splashes on the ground about the camera's point of interest, fading in and out. The
// level greys its sky and lights meanwhile (ApplyWeatherEffects).
#pragma once

// When a house rains (0x0C bytes; LevelRainInfo 80423848, one per house).
struct levelraininfo {
  float m_start = 0;   // +0x00 minute of the day
  float m_end = 0;     // +0x04
  int m_type = 0;      // +0x08 RainEffect::RainType
};

// 0x38 bytes.
struct RainEffect {
  using RainType = ::RainEffect__RainType;
  RainEffect(float fadeIn, float duration, float fadeOut, int type);  // 8007A2D8 (type: RainType)
  RET_UNKNOWN UpdateParticleRainSplashLoad();  // 8007A380
  RET_UNKNOWN UpdateParticleRainLoad();  // 8007A510
  ~RainEffect();  // 8007A64C
  void RainModeUpdate(float dt);  // 8007A76C

  RParticle* m_rainRes = nullptr;    // +0x00
  Emitter* m_rain = nullptr;         // +0x04
  int m_inGlobalSystem = 0;          // +0x08 the rain draws in the global system (camera outside)
  RParticle* m_splashRes = nullptr;  // +0x0C
  Emitter* m_splash = nullptr;       // +0x10
  float m_fadeIn = 0;                // +0x14 minutes
  float m_fadeOut = 0;               // +0x18
  float m_duration = 0;              // +0x1C
  float m_rainMax = 0;               // +0x20 the rain's particle count and rate at full strength
  float m_rainRate = 0;              // +0x24
  float m_splashMax = 0;             // +0x28 (kept, unused)
  float m_splashRate = 0;            // +0x2C
  float m_time = 0;                  // +0x30
  int m_type = 0;                    // +0x34
};

extern levelraininfo LevelRainInfo[16];  // 80423848
bool TimeIsInRange(int time, int start, int end);  // 80024940
void RainSplashParticleBirthCB(pemitter* e, PVector4* pos, PVector4* vel);  // 80079F8C
void RainSplashEmitterProcessCB(void* data);  // 8007A04C
