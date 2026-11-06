// Port of game/weather.cpp (RainEffect and the rain splashes) and the weather of
// game/eorhouse.cpp (EHouse::WeatherChanging / Fair / Rain / UpdateWeather): each house rains at its
// own time of day; the rain fades in and out over ten minutes and the level greys its lights
// meanwhile (ApplyWeatherEffects, e_rlevel.cpp); the house's rain amount (+0x3C) fades the wet
// floors in.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"
#include "simmy/classes/weather.hpp"

#include <cmath>

// 80423848: per house, the minutes of the day it rains (2160: never) and the rain's kind.
levelraininfo LevelRainInfo[16] = {
    {1320.0f, 1439.0f, 2}, {600.0f, 720.0f, 2},   {900.0f, 960.0f, 2},   {1200.0f, 1320.0f, 2},
    {2160.0f, 2160.0f, 2}, {0.0f, 120.0f, 2},     {1260.0f, 1380.0f, 2}, {2160.0f, 2160.0f, 2},
    {1200.0f, 1320.0f, 2}, {720.0f, 840.0f, 2},   {180.0f, 300.0f, 2},   {2160.0f, 2160.0f, 2},
    {600.0f, 660.0f, 2},   {780.0f, 840.0f, 2},   {1260.0f, 1320.0f, 2}, {840.0f, 960.0f, 2},
};

namespace {
// 80423E38 / 80423E44: the rain and splash emitters by rain kind ('PART' ids).
const unsigned int g_RainEmitterIds[3] = {0x81c76719, 0x81c76719, 0x81c76719};
const unsigned int g_RainSplashEmitterIds[3] = {0x5a94379f, 0x5a94379f, 0x5a94379f};
int DAT_804761c0 = 0;  // forced weather (debug): 0 by the table, 1 rain, else fair
}  // namespace

// 80079F8C
// A splash anywhere in the emitter's birth rectangle, at rest.
void RainSplashParticleBirthCB(pemitter* e, PVector4* pos, PVector4* vel) {
  const unsigned int i0 = Effects::gPrecomputedTableIndex & (Effects::kPreComputedTableSize - 1);
  const unsigned int i1 = (i0 + 1) & (Effects::kPreComputedTableSize - 1);
  Effects::gPrecomputedTableIndex = i1 + 1;
  pos->x = e->m_info->m_volume[0] * (Effects::UnitFromBits(Effects::gPrecomputedRand32Table[i0]) - 0.5f);
  pos->y = e->m_info->m_volume[1] * (Effects::UnitFromBits(Effects::gPrecomputedRand32Table[i1]) - 0.5f);
  vel->w = 1.0f;
  vel->z = 0.0f;
  vel->x = 0.0f;
  vel->y = 0.0f;
}

// 8007A04C
// The splashes follow the camera: 20 ahead of it along its view in the ground plane, kept within the
// lot's tiles (less a 10-tile margin), just above the ground, over a 20x20 area of 2x2 sprites.
void RainSplashEmitterProcessCB(void* data) {
  auto* e = static_cast<pemitter*>(data);
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  EHouse* house = _globals.m_house;
  if (!win || !house) return;
  float dx = win->m_lookDir.x, dy = win->m_lookDir.y;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len != 0.0f) {
    dy = dy * (1.0f / len);
    dx = dx * (1.0f / len);
  }
  float p[2] = {win->m_lookPos.x + dx * 20.0f, win->m_lookPos.y + dy * 20.0f};
  const float hi[2] = {
      ((static_cast<float>(Globs_pFixedWorld->GetYSize()) + house->m_posX + 0.5f) - 2.0f) - 10.0f,
      ((static_cast<float>(Globs_pFixedWorld->GetXSize()) + house->m_posY + 0.5f) - 2.0f) - 10.0f};
  const float lo[2] = {house->m_posY + 0.5f + 10.0f, house->m_posX + 0.5f + 10.0f};
  for (int i = 0; i < 2; ++i) {
    const float over = hi[i] - p[i];
    if (over < 0.0f) p[i] += over;
    const float under = lo[i] - p[i];
    if (0.0f < under) p[i] += under;
  }
  e->m_matrix.m[3][1] = p[1];
  e->m_matrix.m[3][0] = p[0];
  e->m_matrix.m[3][2] = 0.06f;
  e->m_info->m_volume[1] = 20.0f;
  e->m_info->m_volume[0] = 20.0f;
  e->m_info->m_sizeY = 2.0f;
  e->m_info->m_sizeX = 2.0f;
  EmitterSpr3dProcessCB(data);
}

// 8007A2D8
RainEffect::RainEffect(float fadeIn, float duration, float fadeOut, int type) {
  m_fadeIn = fadeIn;
  m_duration = duration;
  m_fadeOut = fadeOut;
  m_time = 0.0f;
  m_inGlobalSystem = 0;
  m_type = type;
  m_splash = nullptr;
  m_splashRes = nullptr;
  _particleman.AddRefAsync(g_RainSplashEmitterIds[m_type]);
  m_rain = nullptr;
  m_rainRes = nullptr;
  _particleman.AddRefAsync(g_RainEmitterIds[m_type]);
  UpdateParticleRainLoad();
  SIMMY_STUB("RainEffect: SAnimator2::SetIsRaining (the awareness manager's rain)", 0x8007A2D8);
}

// 8007A380
// Once loaded: the splash emitter, following the camera in the weather system, drawn always; it
// keeps its description's rate and count (and no birth shape: the callback places the splashes).
RET_UNKNOWN RainEffect::UpdateParticleRainSplashLoad() {
  if (m_splashRes) return 1;
  m_splashRes = static_cast<RParticle*>(_particleman.GetRef(g_RainSplashEmitterIds[m_type]));
  if (!m_splashRes) return 0;
  EMat4 id;
  id.Id();
  pemitterinfo* info = m_splashRes->m_info;
  auto* e = new EmitterSpr3d;
  if (e->CreateCameraFollowXY("rainsplash", nullptr, info) == -1) {
    delete e;
    m_splash = nullptr;
    return 1;
  }
  e->Reg(g_pParticleWeatherSystem, false);
  e->SetState(0x4000000, 1);
  m_splash = e;
  e->SetMatrix(&id.m[0][0]);
  pemitterinfo* live = e->Get()->m_info;
  m_splashRate = live->m_rate;
  live->m_birthType = -1;
  m_splashMax = live->m_maxParticles;
  e->SetCallbacks(nullptr, nullptr, RainSplashEmitterProcessCB, RainSplashParticleBirthCB, nullptr, nullptr, nullptr,
                  nullptr, nullptr, 0x1ff & ~0xcu);
  return 1;
}

// 8007A510
// Once loaded: the rain, following the camera (tilted a quarter turn about x), in the weather system,
// drawn always; full strength 1000 particles at 500 a second.
RET_UNKNOWN RainEffect::UpdateParticleRainLoad() {
  if (m_rainRes) return 1;
  m_rainRes = static_cast<RParticle*>(_particleman.GetRef(g_RainEmitterIds[m_type]));
  if (!m_rainRes) return 0;
  EMat4 m;
  m.RotateX(1.5707963705062866f);
  pemitterinfo* info = m_rainRes->m_info;
  auto* e = new EmitterSpr3d;
  if (e->CreateCameraFollowXYZ("rain", nullptr, info) == -1) {
    delete e;
    m_rain = nullptr;
    return 1;
  }
  e->Reg(g_pParticleWeatherSystem, false);
  e->SetState(0x4000000, 1);
  m_rain = e;
  e->SetMatrix(&m.m[0][0]);
  m_rainRate = 500.0f;
  m_rainMax = 1000.0f;
  return 1;
}

// 8007A64C
RainEffect::~RainEffect() {
  if (m_rain) {
    m_rain->UnReg(m_inGlobalSystem ? g_pParticleSystem : g_pParticleWeatherSystem, false);
    delete m_rain;
  }
  _particleman.DelRefAsync(g_RainEmitterIds[m_type]);
  if (m_splash) {
    m_splash->UnReg(g_pParticleWeatherSystem, false);
    delete m_splash;
  }
  _particleman.DelRefAsync(g_RainSplashEmitterIds[m_type]);
}

// 8007A76C
// Each frame (game minutes): the rain fades in over m_fadeIn, holds for m_duration, fades out over
// m_fadeOut (then its emitters die); the rain moves to the global particle system while the camera
// is outside a room (drawn with the scene) and back when it is inside.
void RainEffect::RainModeUpdate(float dt) {
  EHouse* house = _globals.m_house;
  if (!house || !house->m_ready || !UpdateParticleRainLoad()) return;
  UpdateParticleRainSplashLoad();
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (!m_rain || !m_splash || !win) return;
  m_time += dt;
  pemitterinfo* rain = m_rain->Get()->m_info;
  pemitterinfo* splash = m_splash->Get()->m_info;
  const float in = m_fadeIn, hold = m_duration, t = m_time;
  if (t <= in + m_fadeOut + hold) {
    float k = 1.0f;
    if (t < in) k = t / in;
    else if (in + hold < t) k = 1.0f - ((t - in) - hold) / m_fadeOut;
    rain->m_rate = k * m_rainRate;
    rain->m_maxParticles = k * m_rainMax;
    splash->m_rate = k * m_rainRate;
    splash->m_maxParticles = k * m_rainMax;
  } else {
    m_rain->SetState(8, 1);
    m_splash->SetState(8, 1);
  }
  const float fy = win->m_lookPos.y - house->m_posX;
  const float fx = win->m_lookPos.x - house->m_posY;
  int ty = static_cast<int>(fy), tx = static_cast<int>(fx);
  if (0.5f <= fy - static_cast<float>(ty)) ++ty;
  if (0.5f <= fx - static_cast<float>(tx)) ++tx;
  if (ty <= 0 || tx <= 0) return;
  CTilePt tile(ty, tx, 1);
  if (Globs_pFixedWorld->OutOfBounds(tile)) return;
  if (Globs_pFixedWorld->GetRoom(tile) == 0) {
    if (!m_inGlobalSystem) {
      m_rain->UnReg(g_pParticleWeatherSystem, false);
      m_rain->Reg(g_pParticleSystem, false);
      m_inGlobalSystem = 1;
    }
  } else if (m_inGlobalSystem == 1) {
    m_rain->UnReg(g_pParticleSystem, false);
    m_rain->Reg(g_pParticleWeatherSystem, false);
    m_inGlobalSystem = 0;
  }
}

// ---------------------------------------------------------------------------------------------------
// EHouse's weather

// 80024940
// `time` in [start, end] (minutes of the day; the range may wrap past midnight).
bool TimeIsInRange(int time, int start, int end) {
  if (end < start) return start <= time || time <= end;
  return start <= time && time <= end;
}

// 80024828
void EHouse::WeatherChanging() {
  m_field38 = 1;
  m_field34 = SimulatorHour() * 60 + SimulatorMinute();
}

// 800248A8
void EHouse::Fair() {
  if (m_field30 == 0) return;
  m_field30 = 0;
  WeatherChanging();
}

// 800248F4
void EHouse::Rain() {
  if (m_field30 == 1) return;
  m_field30 = 1;
  WeatherChanging();
}

// 8002498C
// In the house's rain time (and 35 minutes past it) the rain runs, else it goes; while the weather
// changes the rain amount (+0x3C, the wet floors' alpha and the level's) fades over 30 minutes in and
// 20 out (after a 5-minute wait).
void EHouse::UpdateWeather() {
  if (!m_level) return;
  const levelraininfo& info = LevelRainInfo[m_house - 1 > 15 ? 15 : (m_house - 1 < 0 ? 0 : m_house - 1)];
  const bool inTime = TimeIsInRange(static_cast<int>(m_minuteOfDay), static_cast<int>(info.m_start),
                                    static_cast<int>(info.m_end + 30.0f + 5.0f)) != 0;
  if ((!inTime || DAT_804761c0 != 0) && DAT_804761c0 != 1) {
    if (m_rain) {
      delete m_rain;
      m_rain = nullptr;
      Fair();
    }
  } else if (!m_rain) {
    float length = info.m_start <= info.m_end ? info.m_end - info.m_start : info.m_end + (1440.0f - info.m_start);
    int type = info.m_type;
    if (DAT_804761c0 == 1) {
      type = 2;
      length = 20.0f;
    }
    m_rain = new RainEffect(10.0f, length, 10.0f, type);
    Rain();
  }
  if (m_field38 == 0) {
    if (m_field30 == 0) m_alpha = 0.0f;
    else if (m_field30 == 1) m_alpha = 1.0f;
  } else {
    const float now = static_cast<float>(SimulatorHour() * 60 + SimulatorMinute()) + static_cast<float>(SimulatorSecond()) / 60.0f;
    const float changed = static_cast<float>(static_cast<unsigned int>(m_field34));
    float t = now;
    if (m_minuteOfDay < changed) t += 1440.0f;
    if (m_field30 == 0) m_alpha = 1.0f - ((t - changed) - 5.0f) / 30.0f;
    else if (m_field30 == 1) m_alpha = ((t - changed) - 5.0f) / 20.0f;
    float a = m_alpha;
    m_alpha = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
  }
  m_level->m_lightAlpha = m_alpha;
}
