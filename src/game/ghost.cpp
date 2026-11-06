// Ghosts (game/esim.cpp, game/simrenderer.cpp, game/e_istaticmodel.cpp): a dead sim's ghost is drawn
// see-through (SimRenderer::RenderGhosted: every body part blended at the level tuning's ghost alpha,
// without z writes, lit by the ghost lights ESim::Draw sets) and wears the ghost effect (a bone
// particle attachment, ESim::UpdateGhostParticleEffect; see boneparticle.cpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"

#include <cstdio>
#include <cstdlib>

// PC: whether the sim is drawn as a ghost (cXPerson vtable 0x21C IsGhost).
// PC development aid: SIMMY_GHOST_TEST=1 makes player 1's selected sim a ghost for the 3D side.
bool ESim::IsGhostSim() {
  static const bool ghostTest = std::getenv("SIMMY_GHOST_TEST") != nullptr;
  if (ghostTest && GetPlayerIndex() == 0) return true;
  return m_person && m_person->IsGhost() != 0;
}

// ---------------------------------------------------------------------------------------------------
// ESim

// 80034C38
// A ghost wears the ghost effect (attachment 0x503FE21C, ghost_fx): each of its nodes attached to its
// bone through the animator (vtable 0xFC); when it stops being one they are detached (0x104) and the
// attachment let go.
void ESim::UpdateGhostParticleEffect() {
  const bool ghost = IsGhostSim();
  SAnimator2* anim = m_person->GetSAnimator();
  if (!ghost) {
    if (!m_ghostEffects) return;
    auto* att = static_cast<REffectsAttachment*>(m_ghostEffects);
    if (anim)
      for (AttachmentNode& node : att->m_nodes)
        anim->DetachParticleEffect(static_cast<unsigned int>(static_cast<signed char>(node.m_node)), node.m_id,
                                   static_cast<signed char>(node.m_type));
    g_effectsAttachmentMan.DelRef(m_ghostEffects, EResourceManager__DelRefMode{1});
    m_ghostEffects = nullptr;
  } else if (!m_ghostEffects) {
    if (!anim) return;  // PC: not before the person has its animator
    m_ghostEffects = g_effectsAttachmentMan.AddRef(0x503FE21Cu, nullptr, 0);
    if (std::getenv("SIMMY_PARTICLE_TRACE"))
      std::printf("[fx] frame %u ghost effect: attachment %s, %zu nodes\n", static_cast<unsigned>(_framecount),
                  m_ghostEffects ? "in" : "missing", m_ghostEffects ? static_cast<REffectsAttachment*>(m_ghostEffects)->m_nodes.size() : size_t{0});
    if (!m_ghostEffects) return;
    auto* att = static_cast<REffectsAttachment*>(m_ghostEffects);
    for (AttachmentNode& node : att->m_nodes)
      anim->AttachParticleEffect(static_cast<unsigned int>(static_cast<signed char>(node.m_node)), node.m_id,
                                 static_cast<signed char>(node.m_type));
  }
}

// 80074D48
// The parts in render order (as Render), each drawn ghosted.
void SimRenderer::RenderGhosted(ERC* rc, EIStaticModel** parts, EMat4* matrices, unsigned int count, bool morphHead) {
  MorphParts(parts, morphHead);
  rc->ModelMatrixList(matrices, static_cast<int>(count));
  for (int part : s_bodyPartRenderOrder)
    if (EIStaticModel* m = parts[part]) m->DrawImmediateGhosted(rc, nullptr);
}

// 8022DC8C
// As DrawImmediate, each shader selected and then: lighting mode 0x40 on, z tested (LEQUAL) without
// writes, no alpha test for alpha-tested layers (flag 4), blended over the frame at the level
// tuning's ghost alpha (ERLevel::s_levelLightingTuning +0x34; TEV blend 0, 2, 2, 1).
void EIStaticModel::DrawImmediateGhosted(ERC* rc, EAnimController* anim) {
  if (!m_model) return;
  if (m_hiddenFn && m_hiddenFn(m_hiddenData)) return;
  EACNodeState* states = anim ? anim->m_states : nullptr;
  const float alpha = ERLevel::s_levelLightingTuning.m_field00[13];
  for (ESubModel* sub : m_model->m_subModels) {
    const int node = static_cast<short>(sub->m_field0C);
    if (node != -1 && states && (states[node].m_flags & EACNodeState::kHidden)) continue;
    for (ESubModelShader* s : sub->m_shaders) {
      const unsigned int color = GetModifiableColor();
      EShader* shader = s->GetEShader();
      if (shader) shader->Select(rc, color);
      rc->EnableRasterModes(0x40, 0);
      rc->ZTest(true, 2, 1, 0);
      if (shader && ((shader->m_layers[0].m_flags | shader->m_layers[1].m_flags) & 4)) rc->AlphaTest(false, 5, 0.5f, 0);
      rc->SetBlendMode(0, 2, 2, 1, alpha, 0);
      rc->SetMipMap(s->m_mipMap, 0);
      rc->DisplayList(s->m_dl);
    }
  }
}
