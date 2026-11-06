// Port of game/aptstring.cpp's text particles: an Apt edit-text word tagged "<PARTICLE_nn>" (nn 0..8)
// carries a little particle effect drawn over it (sparkles, glints, smoke...). AptFormat::
// InitForParticles builds the effect: one or two sprite emitters ('PART' resources) placed in a small
// scene of its own (an E3DWindow looking along +y at them, 60 degree field of view), with the scale
// that maps the word's size to that scene. Once the descriptions are in (CheckAsyncLoads) the emitters
// go into the text particle system (g_pTextParticleSystem, "textpsys", made by AptAuxPCEorGL_Initialize).
// Each time the word is drawn, ProcessParticles points the effect's view at the word's rectangle on
// screen, clears the depth there and runs the text particle system with only this word's emitters
// drawn (their colour keys' alpha under the text's colour transform).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

extern ERFont* gpFont;  // aptauxpceorgl.cpp

// 804FF6DC
psystem* g_pTextParticleSystem = nullptr;

namespace {
AptTextParticle* NewNode() { return new AptTextParticle; }
}  // namespace

// 8000BD74
// The effect of kind `type`: its emitters (the description, their place, scale and turn in the
// effect's scene, an optional colour), the word-to-scene scale, and the camera: at `eye` looking at
// (eye.x, 0, eye.z), z up, 60 degrees at 4:3, 1..100.
void AptFormat::InitForParticles(int type) {
  m_field14 = 1;
  m_field44 = type;
  m_particles = new E3DWindow;  // GC: 0x350 bytes, 16-aligned from the main heap
  AptTextParticle* first = NewNode();
  m_particleList = first;
  EVec3 eye{0, 0, 0};
  auto sizes = [&](float w, float h) {
    m_field58 = w;
    m_field5C = h;
  };
  switch (type) {
    case 0:  // a green streak
      eye = EVec3{0, -5.0f, 0};
      first->m_id = 0x3D0FE2F6;
      first->m_scale = EVec3{8.0f, 1.0f, 0.01f};
      sizes(0.2104f, 0.0438f);
      first->m_color = EVec3{0, 0.55f, 0};
      break;
    case 1: {  // two emitters: a flat sheet behind the word and one further back
      eye = EVec3{0, -5.0f, 0};
      first->m_id = 0xA4905BCB;
      first->m_scale = EVec3{3.5f, 1.0f, 0.01f};
      sizes(0.0905f, 0.0438f);
      first->m_translate = EVec3{0, 0, -0.12f};
      AptTextParticle* second = NewNode();
      first->m_next = second;
      second->m_id = 0xC881D814;
      second->m_scale = EVec3{0.8f, 1.0f, 1.0f};
      second->m_translate = EVec3{0, -3.0f, 0};
      break;
    }
    case 2: {  // two emitters turned 45 degrees each way, either side of the word, at ten times the speed
      m_field54 = 10.0f;
      sizes(0.0905f, 0.0438f);
      eye = EVec3{0, -4.0f, 0};
      first->m_rotate = EVec3{0, 0, 0.7853982f};
      first->m_id = 0x0570E92F;
      first->m_scale = EVec3{0.5f, 1.0f, 0.5f};
      first->m_translate = EVec3{-0.3f, 0, 0};
      AptTextParticle* second = NewNode();
      first->m_next = second;
      second->m_id = 0x0570E92F;
      second->m_rotate = EVec3{0, 0, 0.017453292f * -45.0f};
      second->m_scale = EVec3{0.5f, 1.0f, 0.5f};
      second->m_translate = EVec3{0.3f, 0, 0};
      break;
    }
    case 3:
      eye = EVec3{0, -1.5f, 0};
      first->m_id = 0x954088E4;
      sizes(0.0905f, 0.0438f);
      break;
    case 4:  // a grey stretched glow
      eye = EVec3{0, -1.2f, 0};
      first->m_scale = EVec3{2.5f, 1.0f, 0.8f};
      sizes(0.221f, 0.0638f);
      first->m_color = EVec3{0.35f, 0.35f, 0.35f};
      first->m_id = 0x244012F7;
      first->m_alpha = 0.35f;
      break;
    case 5:
      eye = EVec3{0, -1.3f, 0};
      first->m_id = 0x13C14B2E;
      sizes(0.0905f, 0.0438f);
      break;
    case 6:
      eye = EVec3{0.01f, -1.0f, 0};
      sizes(0.14f, 0.065f);
      first->m_id = 0x2AE3EC55;
      break;
    case 7:
      eye = EVec3{0, -1.1f, 0};
      sizes(0.14f, 0.065f);
      first->m_id = 0x42A7B78C;
      break;
    case 8:
      eye = EVec3{0, -1.1f, 0};
      first->m_id = 0xDAA34611;
      sizes(0.0905f, 0.0438f);
      break;
    default: break;
  }
  for (AptTextParticle* n = m_particleList; n; n = n->m_next)
    n->m_particle = static_cast<RParticle*>(_particleman.AddRefAsync(n->m_id));
  m_particleRefs = 1;
  EVec3 target{eye.x, 0.0f, eye.z};
  EVec3 up{0, 0, 1.0f};
  m_particles->SetLookAt(eye, target, up);
  m_particles->SetProjection(60.0f, 1.3333334f, 1.0f, 100.0f);
  static const bool trace = std::getenv("SIMMY_PARTICLE_TRACE") != nullptr;
  if (trace) std::printf("[fx] frame %u text particles %d\n", static_cast<unsigned>(_framecount), type);
}

// 8000C628
// While the word's format is on and its emitters are made: the effect's view over the word's rectangle
// (the word's centre and size under the text matrix, in the current 3D window's rectangle space, the
// size through the effect's word-to-scene scale, 80 %), the depth cleared there, then the text particle
// system run with this word's emitters drawn, their colour keys' alpha x `alphaScale` + `alphaAdd`;
// the previous window and the font's shader selected again.
void AptFormat::ProcessParticles(ERC* rc, EMat4* matrix, EVec2& pos, EVec2& size, float alphaScale, float alphaAdd) {
  if (!m_field14 || !m_enabled) return;
  if (!m_particlesLoaded && (CheckAsyncLoads(), !m_particlesLoaded)) return;
  auto* cur = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (!cur || !m_particles || !matrix) return;  // PC guard
  const EMat4& n = cur->m_normalizedProjection;
  float cx = pos.x + matrix->m[3][0] + cur->m_viewport.x0;
  float cy = pos.y + matrix->m[3][1] + cur->m_viewport.y0;
  const float w = size.x * matrix->GetScaleX() / m_field58 * 0.8f;
  const float h = size.y * matrix->GetScaleY() / m_field5C * 0.8f * 0.5f;
  const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
  const float y0 = cy - h, y1 = cy + h;
  TRect<float> rect(n.m[2][0] * 0.0f + x0 * n.m[0][0] + y0 * n.m[1][0] + n.m[3][0],
                    n.m[2][1] * 0.0f + x0 * n.m[0][1] + y0 * n.m[1][1] + n.m[3][1],
                    n.m[2][0] * 0.0f + x1 * n.m[0][0] + y1 * n.m[1][0] + n.m[3][0],
                    n.m[2][1] * 0.0f + x1 * n.m[0][1] + y1 * n.m[1][1] + n.m[3][1]);
  m_particles->SetViewport(rect);
  m_particles->Select(rc);
  rc->ZClear(0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  for (AptTextParticle* p = m_particleList; p; p = p->m_next) {
    if (pemitter* e = p->m_emitter ? p->m_emitter->Get() : nullptr) {
      float(&keys)[4][4] = e->m_info->m_color;
      for (int k = 0; k < 4; ++k) {
        p->m_savedAlpha[k] = keys[k][3];
        keys[k][3] = keys[k][3] * alphaScale + alphaAdd;
      }
    }
    if (p->m_emitter) p->m_emitter->SetState(4, 1);
  }
  g_pRC = rc;
  PSystemProcess(_dt * m_field54, g_pTextParticleSystem, 1);
  for (AptTextParticle* p = m_particleList; p; p = p->m_next) {
    if (pemitter* e = p->m_emitter ? p->m_emitter->Get() : nullptr)
      for (int k = 0; k < 4; ++k) e->m_info->m_color[k][3] = p->m_savedAlpha[k];
    if (p->m_emitter) p->m_emitter->SetState(4, 0);
  }
  rc->RenderSurface(nullptr, 5);
  cur->Select(rc);
  gpFont->Select(rc);
}

// 8000CB00
// Once every emitter's description is in: each a sprite emitter ("font tag emitter") in the text
// particle system, placed by its scale, turns (x, y, z) and offset, its colour keys' rgb set when it
// has a colour, not drawn until ProcessParticles. 1: made.
int AptFormat::CheckAsyncLoads() {
  int loaded = 1;
  for (AptTextParticle* p = m_particleList; p; p = p->m_next) {
    if (!p->m_particle) {
      p->m_particle = static_cast<RParticle*>(_particleman.GetRef(p->m_id));
      if (!p->m_particle) {
        loaded = 0;
        break;
      }
    }
  }
  if (loaded) {
    for (AptTextParticle* p = m_particleList; p; p = p->m_next) {
      pemitterinfo* info = p->m_particle->m_info;
      auto* e = new EmitterSpr3d;
      p->m_emitter = e;
      e->Create("font tag emitter", nullptr, info);
      e->EmitterSetPostDestroyCallback(ParticleManDelRefCB, p->m_particle);
      e->Reg(g_pTextParticleSystem, false);
      EMat4 m;
      m.Scale(p->m_scale);
      m.PostRotateX(p->m_rotate.x);
      m.PostRotateY(p->m_rotate.y);
      m.PostRotateZ(p->m_rotate.z);
      m.PostTranslate(p->m_translate);
      e->SetMatrix(&m.m[0][0]);
      if (p->m_color.x >= 0.0f) {
        if (pemitter* pe = e->Get()) {
          for (int k = 0; k < 4; ++k) {
            pe->m_info->m_color[k][0] = p->m_color.x;
            pe->m_info->m_color[k][1] = p->m_color.y;
            pe->m_info->m_color[k][2] = p->m_color.z;
          }
        }
      }
      e->SetState(4, 0);
    }
  }
  m_particlesLoaded = loaded;
  return loaded;
}

// 8000B1B0 (the particle part)
// The view goes; each emitter leaves the text particle system and is destroyed; the descriptions asked
// for are let go; the list is freed.
void AptFormat::ReleaseParticles() {
  if (m_particles) {
    delete m_particles;
    m_particles = nullptr;
  }
  for (AptTextParticle* p = m_particleList; p; p = p->m_next) {
    if (!p->m_emitter) continue;
    p->m_emitter->UnReg(g_pTextParticleSystem, false);
    static_cast<EmitterSpr3d*>(p->m_emitter)->Destroy();
    delete p->m_emitter;
    p->m_emitter = nullptr;
  }
  if (m_particleRefs) {
    for (AptTextParticle* p = m_particleList; p; p = p->m_next) _particleman.DelRefAsync(p->m_id);
    m_particleRefs = 0;
  }
  while (m_particleList) {
    AptTextParticle* next = m_particleList->m_next;
    delete m_particleList;
    m_particleList = next;
  }
}

// PC development aid: SIMMY_TEXT_PARTICLE_TEST=<type>:<word> gives every Apt text word <word> the text
// particle effect <type> (0..8), as a "<PARTICLE_0n>" tag before it would (the game's own strings with
// these tags are left-overs that the shipped screens do not show).
void AptTextParticleTest(AptParagraph* paragraph) {
  static const char* test = std::getenv("SIMMY_TEXT_PARTICLE_TEST");
  if (!test || !paragraph) return;
  const int type = std::atoi(test);
  const char* word = std::strchr(test, ':');
  if (!word || type < 0 || type > 8) return;
  ++word;
  for (AptLine* line = paragraph->m_first; line; line = line->m_next)
    for (AptWord* w = line->m_first; w; w = w->m_next) {
      if (!w->m_text) continue;
      size_t i = 0;
      while (word[i] && w->m_text[i] == static_cast<unsigned char>(word[i])) ++i;
      if (word[i] || (w->m_text[i] && w->m_text[i] != ' ')) continue;
      if (w->m_format && w->m_format->m_particleList) continue;
      w->ForceFormat(nullptr);
      w->m_format->InitForParticles(type);
    }
}
