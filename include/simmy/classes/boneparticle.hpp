// @class AnimParticleData after Effects__Effect__ClientParams
// @class EBoneParticle after Effects__Effect__ClientParams AnimParticleData
// Particle effects on a sim's bones (game/boneparticle.cpp): an animation's particle note (kind 0xA:
// an effects attachment by name), the ghost effect (ESim::UpdateGhostParticleEffect) and the
// animator's Attach/DetachParticleEffect make one EBoneParticle per attachment node; SAnimator2 keeps
// them in two trees (node -> effect id -> EBoneParticle; +0x5D0 the main animation's, +0x5DC the
// overlay's) and places each on its bone every frame (SAnimator2::updateParticles).
// Kind 0 is an emitter of the older particle system (sprites in g_pParticleSystem), kind 1 an
// Effects:: effect made with this object's client parameters.
#pragma once

// What a particle note asks for (0x0C bytes).
struct AnimParticleData {
  int m_node = 0;              // +0x00 the bone (EAnimController node)
  unsigned int m_id = 0;       // +0x04 the 'PART' or effects-emitter resource
  unsigned short m_type = 0;   // +0x08 the attachment node's kind and flags (AttachmentNode +0x00)
  short m_sortLayer = 0;       // +0x0A
};

// 0x80 bytes (allocated 16-aligned). Starts with the client parameters an effects emitter is made with.
struct EBoneParticle : public Effects__Effect__ClientParams {
  EBoneParticle(cXPerson* person, AnimParticleData* data, bool keep);  // 80015074
  // Once its resource is in: the emitter or effect made. 1: ready (or failed for good).
  int UpdateParticleLoad();  // 800151C4
  ~EBoneParticle();  // 80015358
  // Placed on its bone (hidden while the person's data 0x22 is set).
  void Update();  // 80015504

  cXPerson* m_person = nullptr;          // +0x50
  int m_node = 0;                        // +0x54
  unsigned int m_id = 0;                 // +0x60
  int m_type = 0;                        // +0x64 0 particle emitter, 1 effects emitter
  Emitter* m_emitter = nullptr;          // +0x68
  Effects__Effect* m_effect = nullptr;   // +0x6C
  RParticle* m_particle = nullptr;       // +0x70
  REffectsEmitter* m_effectsEmitter = nullptr;  // +0x74
  int m_immediate = 0;                   // +0x78 deleted at once (else its particles finish)
  int m_keep = 0;                        // +0x7C attached by AttachParticleEffect (kept over animation changes)
};
