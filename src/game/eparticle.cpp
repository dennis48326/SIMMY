// Port of game/iobject.cpp's particle effects: EParticleEffect (one effect of an attachment: a
// particle Emitter or an Effects::Effect, loaded asynchronously, placed at the object or one of its
// model's nodes each frame) and EParticleObj (the effects of one REffectsAttachment). ISimsObjectModel
// makes them for its state's ObjAnimDef and for animation events (isimsobjectmodel.cpp).
//
// Kind 1 is an effects emitter of the newer system (Effects::EffectsManager / FastParticleEmitter,
// src/engine/fastparticleemitter.cpp), made with this effect's client parameters.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"

#include <cstdio>
#include <cstdlib>

// 8004B8E0
// The node's effect (its id, kind, sort flag) at `m`; the resource is asked for at once.
EParticleEffect::EParticleEffect(AttachmentNode& node, EMat4& m) {
  m_sortPos = nullptr;
  m_dofForeground = 0;
  m_sortByEmitter = 0;
  m_sortLayer = 0;
  m_matrix.Id();
  m_id = node.m_id;
  m_type = static_cast<signed char>(node.m_type & ~AttachmentNode::kUseEmitterPosForAlphaSort);
  m_sortLayer = node.m_field02;
  m_sortByEmitter = (node.m_type & AttachmentNode::kUseEmitterPosForAlphaSort) != 0;
  m_matrix = m;
  if (m_type == 0) _particleman.AddRefAsync(m_id);
  else if (m_type == 1) g_effectsEmitterMan.AddRefAsync(m_id);
  UpdateParticleEffectLoad();
}

// 8004BA38
// Once the resource is in: a particle emitter of the description's kind (sprites into the global
// system, models into the model system), its description released when it goes, not emitting until
// placed. 1: ready (or failed for good).
int EParticleEffect::UpdateParticleEffectLoad() {
  if (m_type != 0) {
    if (m_type == 1) {
      if (m_effectsEmitter) return 1;
      m_effectsEmitter = static_cast<REffectsEmitter*>(g_effectsEmitterMan.GetRef(m_id));
      if (m_effectsEmitter) {
        static const bool trace = std::getenv("SIMMY_PARTICLE_TRACE") != nullptr;
        m_effect = Effects__EffectsManager::GetSingleton()->CreateEffect(*m_effectsEmitter, this);
        if (trace)
          std::printf("[fx] frame %u effects emitter 0x%08x: %s\n", static_cast<unsigned>(_framecount), m_id,
                      m_effect ? "running" : "failed");
        if (m_effect) {
          // Neither updated nor drawn until placed (SetPos).
          m_effect->m_flags &= ~(Effects__Effect::kUpdate | Effects__Effect::kRender);
          return 1;
        }
      }
    }
    return 0;
  }
  if (m_particle) return 1;
  m_particle = static_cast<RParticle*>(_particleman.GetRef(m_id));
  if (!m_particle) return 0;
  pemitterinfo* info = m_particle->m_info;
  char name[32];
  std::snprintf(name, sizeof(name), "0x%x", m_id);  // 803D315C
  // PC development aid: SIMMY_PARTICLE_TRACE=1 logs the emitters made.
  static const bool trace = std::getenv("SIMMY_PARTICLE_TRACE") != nullptr;
  if (trace)
    std::printf("[fx] frame %u emitter 0x%08x: flags 0x%x birth %d rate %.1f max %.0f shader %s\n", static_cast<unsigned>(_framecount), m_id, info->m_flags,
                info->m_birthType, info->m_rate, info->m_maxParticles, info->m_shaderName);
  int r;
  if (info->m_flags & 0x2000000) {
    auto* e = new EmitterGeom3dTab;
    m_emitter = e;
    r = e->Create(name, nullptr, info);
  } else if (info->m_flags & 0x200000) {
    auto* e = new EmitterGeom3d;
    m_emitter = e;
    r = e->Create(name, nullptr, info);
  } else {
    auto* e = new EmitterSpr3d;
    m_emitter = e;
    r = e->Create(name, nullptr, info);
    if (r != -1) {
      m_emitter->Reg(g_pParticleSystem, true);
      m_emitter->EmitterSetPostDestroyCallback(ParticleManDelRefCB, m_particle);
      m_emitter->SetState(2, 0);
      return 1;
    }
  }
  if (r != -1) {
    if (info->m_flags & 0x2200000) {
      m_emitter->Reg(g_pParticleModelSystem, true);
      m_emitter->EmitterSetPostDestroyCallback(ParticleManDelRefCB, m_particle);
      m_emitter->SetState(2, 0);
      return 1;
    }
  }
  delete m_emitter;
  m_emitter = nullptr;
  return 1;
}

// 8004BC6C
// A placed emitter dies (out of the level; its system lets its particles finish, then releases
// the description); one still loading drops its reference.
EParticleEffect::~EParticleEffect() {
  if (!m_emitter) {
    if (m_effect) {
      // Let go: it stops bursting, and the manager deletes it once its particles are gone.
      // PC: it no longer sorts at our point (freed with us; GC kept the stale pointer).
      m_effect->m_flags |= Effects__Effect::kReleased | Effects__Effect::kUpdate;
      m_effect->m_sortPos = nullptr;
      m_effect->Stop();
      m_effect = nullptr;
    } else if (m_id && m_type == 0) {
      _particleman.DelRefAsync(m_id);
      m_id = 0;
      m_particle = nullptr;
    }
  } else {
    m_emitter->SetState(0x4000, 1);
    m_emitter->SetState(8, 1);
    delete m_emitter;
    m_id = 0;
    m_emitter = nullptr;
    m_particle = nullptr;
  }
  if (m_id && m_type == 1) {
    g_effectsEmitterMan.DelRefAsync(m_id);
    m_id = 0;
    m_effectsEmitter = nullptr;
  }
}

// 8004BDAC
// The emitter emits from where it is placed: its description's own matrix (its translation only
// with flag 0x8000) on `m` (its rotation only with flag 0x10000); sorted at `sortPos`.
void EParticleEffect::SetPos(EMat4& m, EVec3* sortPos) {
  if (m_type != 0) {
    if (m_type == 1 && m_effect) {
      m_effect->m_flags |= Effects__Effect::kRender;
      m_effect->m_flags |= Effects__Effect::kUpdate;
      m_effect->SetParentTransform(m);
      m_effect->m_sortPos = sortPos ? &sortPos->x : nullptr;
    }
    return;
  }
  if (!m_emitter) return;
  m_emitter->SetState(2, 1);
  m_emitter->SetSortPos(*sortPos);
  pemitterinfo* info = m_emitter->Get()->m_info;
  EMat4 obj = m;
  EMat4 local = info->m_matrix;
  if (!(info->m_flags & 0x10000)) {
    obj.m[2][1] = 0.0f, obj.m[2][2] = 1.0f, obj.m[0][0] = 1.0f, obj.m[0][1] = 0.0f, obj.m[0][2] = 0.0f;
    obj.m[1][0] = 0.0f, obj.m[1][1] = 1.0f, obj.m[1][2] = 0.0f, obj.m[2][0] = 0.0f;
  }
  if (!(info->m_flags & 0x8000)) local.m[3][0] = local.m[3][1] = local.m[3][2] = 0.0f;
  EMat4 out;
  out.Mult4x4(local, obj);
  m_emitter->SetMatrix(&out.m[0][0]);
}

// 8004BFE4
// Drawn while the object is visible at all.
void EParticleEffect::SetAlphaFade(float alpha) {
  if (m_type == 0) {
    if (m_emitter) m_emitter->SetState(4, 0.004999999888241291 <= alpha ? 1 : 0);
  } else if (m_type == 1 && m_effect) {
    static_cast<Effects__FastParticleEmitter*>(m_effect)->m_alphaScale = alpha;
  }
}

// 8004C060 CalcRotAngleOff (unused here)

// 8004C130
EParticleObj::EParticleObj() {}

// 8004C150
EParticleObj::~EParticleObj() {
  if (m_owns)
    for (EParticleEffect* e : m_effects) delete e;
  m_effects.clear();
  if (m_attachment) {
    m_attachment->DelRef();
    m_attachment = nullptr;
  }
}

// 8004C1F8
// The attachment of `def` (its +0x04) and an effect per node that names a resource. 0: none.
int EParticleObj::CreateEffects(EMat4& m, float, ObjAnimDef* def) {
  if (m_owns)
    for (EParticleEffect* e : m_effects) delete e;
  m_effects.clear();
  if (m_attachment) {
    m_attachment->DelRef();
    m_attachment = nullptr;
  }
  if (!def->m_field04) return 0;
  m_attachment = static_cast<REffectsAttachment*>(g_effectsAttachmentMan.AddRef(def->m_field04, nullptr, 0));
  if (!m_attachment) return 0;
  if (m_attachment->m_nodes.empty()) {
    m_attachment->DelRef();
    m_attachment = nullptr;
    return 0;
  }
  static const bool trace = std::getenv("SIMMY_PARTICLE_TRACE") != nullptr;
  for (AttachmentNode& node : m_attachment->m_nodes) {
    if (trace)
      std::printf("[fx] attachment 0x%08x: node type %d bone %d id 0x%08x\n", def->m_field04, node.m_type, node.m_node,
                  node.m_id);
    if (node.m_id) m_effects.push_back(new EParticleEffect(node, m));
  }
  return 1;
}

// 8004C350
// Each loaded effect placed: at the node of the object's model it follows (when the model has a
// skeleton), else at the object; drawn with the object's fade. (GC: the attachment's nodes are read
// in step with the list of effects.)
void EParticleObj::UpdateEffectPosAndAlphaFade(EAnimController& anim, float, EMat4& m, EVec3* sortPos, float alpha) {
  for (size_t i = 0; i < m_effects.size(); ++i) {
    EParticleEffect* e = m_effects[i];
    if (!e || !e->UpdateParticleEffectLoad()) continue;
    if (!m_attachment || i >= m_attachment->m_nodes.size()) continue;
    const AttachmentNode& node = m_attachment->m_nodes[i];
    if (!(node.m_node & 0x80)) {
      if (anim.m_character && anim.NodeCount() > 0) {
        EMat4 at = m;
        anim.CalcNodeOrient(static_cast<signed char>(node.m_node), at);
        e->SetPos(at, sortPos);
        e->SetAlphaFade(alpha);
      }
    } else {
      e->SetPos(m, sortPos);
      e->SetAlphaFade(alpha);
    }
  }
}
