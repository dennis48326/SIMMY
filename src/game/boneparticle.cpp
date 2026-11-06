// Port of game/boneparticle.cpp (EBoneParticle) and SAnimator2's bone particles (game/sanimator2.cpp:
// the particle note, Attach/DetachParticleEffect, the per-frame placement and the cleanups).
//
// A particle note (animation note kind 0xA) names an effects attachment ('EATC'); each node of it that
// names a resource and a bone (> 0) toggles an EBoneParticle on that bone: the first note of a pair
// starts it, the second ends it (procBoneParticleEvt). The animator keeps them per bone and id in two
// trees (+0x5D0 for the main animation, +0x5DC for the overlay's notes, flag 0x80 while those run);
// when the main animation changes (flag 0x10000) or the overlay ends (0x20000) the note-made ones go
// (cleanupParticlesDelayed: their particles finish), the attached ones (AttachParticleEffect, the ghost
// effect) stay until detached. A selected or socialising sim's effects go to the depth-of-field
// foreground (kSendEmitterPosToForeground).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

namespace {
// AttachmentNode::kSendEmitterPosToForeground (8050110D).
constexpr unsigned short kSendEmitterPosToForeground = 0x40;

cXPersonImpl* PersonImpl(cXPerson* person) { return static_cast<cXPersonImpl*>(person); }

// The person's vtable 0x11C IsSelected (8012953C) or 0x334 IsInSocialMode (8012CA2C).
// PC: IsSelected is "a player's selected person" (as autonomy.cpp and sprites.cpp have it).
bool SelectedOrSocial(cXPerson* person) {
  if (!person) return false;
  if (_globals.GetSelectedPerson(0) == person || _globals.GetSelectedPerson(1) == person) return true;
  cXPersonImpl* impl = PersonImpl(person);
  return impl && impl->IsInSocialMode() != 0;
}

// The person's object data 0x22 (cXObject vtable 0x264): hidden.
bool Hidden(cXPerson* person) {
  cXPersonImpl* impl = PersonImpl(person);
  return impl && impl->GetData(0x22) != 0;
}

// PC: an EBoneParticle's delete (GC: ~EBoneParticle with 3, the 16-aligned main-heap block freed).
void DeleteBoneParticle(EBoneParticle* p) {
  if (!p) return;
  p->~EBoneParticle();
  ::operator delete(p, std::align_val_t{16});
}

bool Trace() {
  static const bool trace = std::getenv("SIMMY_PARTICLE_TRACE") != nullptr;
  return trace;
}
}  // namespace

// 80015074
// The effect `data` names on its bone; `keep`: attached for good (not by a note). Kind 1 tells the
// animator's bone position at once (GetBonePosAndDirForParticle into the client matrix).
EBoneParticle::EBoneParticle(cXPerson* person, AnimParticleData* data, bool keep) {
  m_sortPos = nullptr;
  m_dofForeground = 0;
  m_sortByEmitter = 0;
  m_sortLayer = 0;
  m_matrix.Id();
  m_person = person;
  m_node = data->m_node;
  m_emitter = nullptr;
  m_effect = nullptr;
  m_particle = nullptr;
  m_effectsEmitter = nullptr;
  m_id = data->m_id;
  m_type = static_cast<short>(data->m_type) & ~static_cast<int>(AttachmentNode::kUseEmitterPosForAlphaSort | kSendEmitterPosToForeground);
  m_sortLayer = data->m_sortLayer;
  m_sortByEmitter = (data->m_type & AttachmentNode::kUseEmitterPosForAlphaSort) != 0;
  m_dofForeground = (data->m_type & kSendEmitterPosToForeground) != 0;
  m_keep = keep ? 1 : 0;
  m_immediate = 0;
  if (m_type == 0) {
    _particleman.AddRefAsync(m_id);
  } else if (m_type == 1) {
    if (person)
      if (SAnimator2* anim = person->GetSAnimator()) anim->GetBonePosAndDirForParticle(static_cast<unsigned int>(m_node), m_matrix);
    g_effectsEmitterMan.AddRefAsync(m_id);
  }
  UpdateParticleLoad();
}

// 800151C4
// Kind 0: a sprite emitter into the global particle system (in the foreground list when flagged), its
// description released when it goes. Kind 1: the effect, made with this object's client parameters.
int EBoneParticle::UpdateParticleLoad() {
  if (m_type == 0) {
    if (m_particle) return 1;
    m_particle = static_cast<RParticle*>(_particleman.GetRef(m_id));
    if (!m_particle) return 0;
    pemitterinfo* info = m_particle->m_info;
    char name[32];
    std::snprintf(name, sizeof(name), "0x%x", m_id);  // 803CE0C4
    auto* e = new EmitterSpr3d;
    m_emitter = e;
    if (e->Create(name, nullptr, info) == -1) {
      delete m_emitter;
      m_emitter = nullptr;
      return 1;
    }
    if (Trace())
      std::printf("[fx] frame %u bone %d emitter 0x%08x (flags 0x%x)\n", static_cast<unsigned>(_framecount), m_node, m_id, info->m_flags);
    if (m_dofForeground) m_emitter->SetState(0x8000000, 1);
    m_emitter->EmitterSetPostDestroyCallback(ParticleManDelRefCB, m_particle);
    m_emitter->Reg(g_pParticleSystem, true);
    return 1;
  }
  if (m_type == 1) {
    if (m_effectsEmitter) return 1;
    m_effectsEmitter = static_cast<REffectsEmitter*>(g_effectsEmitterMan.GetRef(m_id));
    if (m_effectsEmitter) {
      m_effect = Effects__EffectsManager::GetSingleton()->CreateEffect(*m_effectsEmitter, this);
      if (Trace())
        std::printf("[fx] frame %u bone %d effects emitter 0x%08x: %s\n", static_cast<unsigned>(_framecount), m_node, m_id,
                    m_effect ? "running" : "failed");
      return m_effect ? 1 : 0;
    }
  }
  return 0;
}

// 80015358
// An emitter: stopped; deleted at once (out of its system, its description released) or left to its
// system to finish its particles. An effect: deleted at once, or let go (it stops and the manager
// deletes it once done). Still loading: the reference dropped.
EBoneParticle::~EBoneParticle() {
  m_person = nullptr;
  if (m_emitter) {
    m_emitter->SetState(8, 1);
    if (m_immediate == 0) {
      m_emitter->SetState(0x4000, 1);
    } else {
      m_emitter->SetState(0x4000, 0);
      m_emitter->UnReg(g_pParticleSystem, true);
      _particleman.DelRef(static_cast<EResource*>(m_particle), EResourceManager__DelRefMode{1});
    }
    delete m_emitter;
    m_id = 0;
    m_emitter = nullptr;
    m_particle = nullptr;
  } else if (m_effect) {
    if (m_immediate == 0) {
      // PC: it no longer points at this object's sort point (GC kept the stale pointer).
      m_effect->m_flags |= Effects__Effect::kReleased;
      m_effect->m_sortPos = nullptr;
      m_effect->Stop();
    } else {
      Effects__EffectsManager::GetSingleton()->DeleteEffect(*m_effect);
    }
    m_effectsEmitter = nullptr;
    m_effect = nullptr;
  } else {
    if (m_id == 0) return;
    if (m_type == 0) {
      _particleman.DelRefAsync(m_id);
      m_id = 0;
      m_particle = nullptr;
    }
  }
  if (m_id != 0 && m_type == 1) {
    g_effectsEmitterMan.DelRefAsync(m_id);
    m_id = 0;
    m_effectsEmitter = nullptr;
  }
}

// 80015504
// On the bone's matrix (the animator's GetBonePosAndDirForParticle; identity without a person). An
// emitter: its description's own matrix (translation only with flag 0x8000) on the bone's (rotation
// only with flag 0x10000), emitting unless the person is hidden. An effect: drawn and updated unless
// hidden, its parent the bone.
void EBoneParticle::Update() {
  if (!UpdateParticleLoad()) return;
  EMat4 bone;
  if (!m_person) {
    bone.Id();
  } else {
    bone.Id();
    if (SAnimator2* anim = m_person->GetSAnimator()) anim->GetBonePosAndDirForParticle(static_cast<unsigned int>(m_node), bone);
  }
  if (m_type == 0) {
    if (!m_emitter) return;
    if (!m_person) {
      m_emitter->SetState(2, 1);
    } else {
      if (Hidden(m_person)) {
        m_emitter->SetState(2, 0);
        return;
      }
      m_emitter->SetState(2, 1);
    }
    pemitterinfo* info = m_emitter->Get()->m_info;
    EMat4 obj = bone;
    EMat4 local = info->m_matrix;
    if (!(info->m_flags & 0x10000)) {
      obj.m[2][1] = 0.0f, obj.m[2][2] = 1.0f, obj.m[0][0] = 1.0f, obj.m[0][1] = 0.0f, obj.m[0][2] = 0.0f;
      obj.m[1][0] = 0.0f, obj.m[1][1] = 1.0f, obj.m[1][2] = 0.0f, obj.m[2][0] = 0.0f;
    }
    if (!(info->m_flags & 0x8000)) local.m[3][0] = local.m[3][1] = local.m[3][2] = 0.0f;
    EMat4 out;
    out.Mult4x4(local, obj);
    m_emitter->SetMatrix(&out.m[0][0]);
  } else if (m_type == 1 && m_effect) {
    if (m_person) {
      if (!Hidden(m_person)) m_effect->m_flags |= Effects__Effect::kRender | Effects__Effect::kUpdate;
      else m_effect->m_flags &= ~(Effects__Effect::kRender | Effects__Effect::kUpdate);
    }
    m_effect->SetParentTransform(bone);
  }
}

// ---------------------------------------------------------------------------------------------------
// SAnimator2

// 8005C87C
// The main animation changed (0x10000) or the overlay ended (0x20000): their note-made particles go.
void SAnimator2::checkParticleCleanup() {
  if (m_flags & 0x10000) {
    cleanupParticlesDelayed(&m_particles);
    m_flags &= ~0x10000u;
  }
  if (m_flags & 0x20000) {
    cleanupParticlesDelayed(&m_overlayParticles);
    m_flags &= ~0x20000u;
  }
}

// 80061658
// Every bone particle placed; in the depth-of-field foreground while the person is selected or
// socialising.
void SAnimator2::updateParticles() {
  // PC development aid: SIMMY_BONE_FX=<attachment>[,<attachment>...] attaches those effects attachments'
  // nodes to player 1's selected sim (as AttachParticleEffect does), 120 frames after it is first placed.
  static const char* boneFx = std::getenv("SIMMY_BONE_FX");
  if (boneFx && m_person && _globals.GetSelectedPerson(0) == m_person) {
    static int wait = 120;
    if (wait > 0 && --wait == 0) {
      std::string names(boneFx);
      for (size_t at = 0; at <= names.size();) {
        size_t end = names.find(',', at);
        if (end == std::string::npos) end = names.size();
        std::string name = names.substr(at, end - at);
        if (EResource* res = g_effectsAttachmentMan.AddRef(const_cast<char*>(name.c_str()), nullptr, 0)) {
          for (AttachmentNode& node : static_cast<REffectsAttachment*>(res)->m_nodes)
            AttachParticleEffect(static_cast<unsigned int>(static_cast<signed char>(node.m_node)), node.m_id,
                                 static_cast<signed char>(node.m_type));
          res->DelRef();
        } else {
          std::printf("[fx] SIMMY_BONE_FX: no attachment %s\n", name.c_str());
        }
        at = end + 1;
      }
    }
  }
  const int foreground = SelectedOrSocial(m_person) ? 1 : 0;
  for (auto& bone : m_particles) {
    for (auto& entry : *bone.second) {
      EBoneParticle* p = entry.second;
      p->Update();
      if (p->m_effect) p->m_effect->m_dofForeground = foreground;
      else if (p->m_emitter) p->m_emitter->SetState(0x8000000, foreground);
      else p->m_dofForeground = foreground;
    }
    // GC: also normalises the carry direction (+0x140, PSVECNormalize) once per bone. PC: not a zero
    // vector (PSVECNormalize would make it NaN).
    const float len = std::sqrt(m_carryDir.x * m_carryDir.x + m_carryDir.y * m_carryDir.y + m_carryDir.z * m_carryDir.z);
    if (len > 0.0f) m_carryDir = EVec3(m_carryDir.x / len, m_carryDir.y / len, m_carryDir.z / len);
  }
}

// 800653F8
// The bone's world matrix (the controller's node orientation).
void SAnimator2::GetBonePosAndDirForParticle(unsigned int node, EMat4& out) {
  m_sim->m_anim.CalcNodeOrient(static_cast<int>(node), out);
}

// 80065420
// Attaches the effect `id` (kind `type`) to `node` for good, unless the bone already has one.
void SAnimator2::AttachParticleEffect(unsigned int node, unsigned int id, int type) {
  if (id == 0) return;
  if (m_particles.find(node) != m_particles.end()) return;
  AnimParticleData data;
  data.m_node = static_cast<int>(node);
  data.m_id = id;
  data.m_type = static_cast<unsigned short>(type);
  if (SelectedOrSocial(m_person)) data.m_type |= kSendEmitterPosToForeground;
  procBoneParticleEvt(&data, true);
}

// 800654F0
// Toggles the effect off again (the same call as attaching finds it and removes it).
void SAnimator2::DetachParticleEffect(unsigned int node, unsigned int id, int type) {
  if (id == 0) return;
  AnimParticleData data;
  data.m_node = static_cast<int>(node);
  data.m_id = id;
  data.m_type = static_cast<unsigned short>(type);
  if (SelectedOrSocial(m_person)) data.m_type |= kSendEmitterPosToForeground;
  procBoneParticleEvt(&data, false);
}

// 8006559C
RET_UNKNOWN SAnimator2::StartParticleEffectFromEdithPrimitive(unsigned int, unsigned int) { return {}; }

// 80065B68
// The resources the animation's notes use loaded ahead: sounds (kinds 5) and particles (kind 0xA).
void SAnimator2::PreloadEvents(AnimRef* ref) {
  EResource* res = ref ? _animman.GetRef(AnimRefId(ref)) : nullptr;
  if (!res) return;
  ERAnim* anim = static_cast<ERAnim*>(res);
  for (EAnimNote& note : anim->m_notes) {
    if (note.m_field04 == 5) {
      g_soundeventman.PreloadResource(const_cast<char*>(note.m_text.c_str()));
    } else if (note.m_field04 == 10) {
      EResource* att = g_effectsAttachmentMan.AddRef(const_cast<char*>(note.m_text.c_str()), nullptr, 0);
      if (att) {
        PreloadBoneParticleEvent(static_cast<REffectsAttachment*>(att));
        att->DelRef();
      }
    }
  }
}

// 80065C70
// GC: every node with a resource preloads it from the particle manager (whatever its kind).
void SAnimator2::PreloadBoneParticleEvent(REffectsAttachment* att) {
  for (AttachmentNode& node : att->m_nodes)
    if (node.m_id != 0) _particleman.PreloadResource(node.m_id);
}

// 80065CE4
// A particle note: each node of the attachment with a resource and a bone toggles its effect there.
void SAnimator2::_handleParticleEvent(REffectsAttachment* att) {
  for (AttachmentNode& node : att->m_nodes) {
    if (node.m_id == 0) continue;
    const int bone = static_cast<signed char>(node.m_node);
    if (bone <= 0) continue;
    AnimParticleData data;
    data.m_node = bone;
    data.m_id = node.m_id;
    data.m_type = static_cast<unsigned short>(static_cast<signed char>(node.m_type));
    if (SelectedOrSocial(m_person)) data.m_type |= kSendEmitterPosToForeground;
    // GC: +0x0A (the sort layer) is left as the stack had it.
    data.m_sortLayer = 0;
    procBoneParticleEvt(&data, false);
  }
}

// 80065DE8
// Toggles `data`'s effect on its bone in the current tree (the overlay's while its notes run): made
// if the bone does not have it, else deleted (its particles finish).
void SAnimator2::procBoneParticleEvt(AnimParticleData* data, bool keep) {
  if (!data) return;
  if (Trace())
    std::printf("[fx] frame %u bone particle %s: bone %d id 0x%08x kind 0x%x%s\n", static_cast<unsigned>(_framecount),
                keep ? "attach" : "note", data->m_node, data->m_id, data->m_type, (m_flags & 0x80) ? " (overlay)" : "");
  BoneParticleTree& tree = (m_flags & 0x80) ? m_overlayParticles : m_particles;
  const unsigned int node = static_cast<unsigned int>(data->m_node);
  const unsigned int id = data->m_id;
  auto make = [&] {
    void* mem = ::operator new(sizeof(EBoneParticle), std::align_val_t{16});
    auto* p = new (mem) EBoneParticle(m_person, data, keep);
    p->m_dofForeground = SelectedOrSocial(m_person) ? 1 : 0;
    return p;
  };
  auto it = tree.find(node);
  if (it == tree.end()) {
    auto* ids = new std::map<unsigned int, EBoneParticle*>;
    tree.emplace(node, ids);
    ids->emplace(id, make());
    return;
  }
  auto* ids = it->second;
  auto found = ids->find(id);
  if (found == ids->end()) {
    ids->emplace(id, make());
  } else {
    EBoneParticle* p = found->second;
    ids->erase(found);
    p->m_immediate = 0;
    DeleteBoneParticle(p);
  }
}

// 80065FF4
// Every particle of `tree` lets its particles finish, then the note-made ones go.
void SAnimator2::cleanupParticlesDelayed(BoneParticleTree* tree) {
  for (auto& bone : *tree)
    for (auto& entry : *bone.second) entry.second->m_immediate = 0;
  cleanupParticles(tree);
}

// 80066054
// All of `tree` goes at once (the animator's end).
void SAnimator2::cleanupParticlesImmediate(BoneParticleTree* tree) {
  for (auto& bone : *tree)
    for (auto& entry : *bone.second) {
      entry.second->m_keep = 0;
      entry.second->m_immediate = 1;
    }
  cleanupParticles(tree);
}

// 800660BC
// A bone whose particles are all note-made loses them; the tree is emptied once nothing is kept.
void SAnimator2::cleanupParticles(BoneParticleTree* tree) {
  bool allGone = true;
  for (auto& bone : *tree) {
    auto* ids = bone.second;
    bool clear = true;
    for (auto& entry : *ids)
      if (entry.second->m_keep != 0) {
        clear = false;
        allGone = false;
      }
    if (clear) {
      for (auto& entry : *ids) DeleteBoneParticle(entry.second);
      ids->clear();
    }
  }
  if (allGone) {
    for (auto& bone : *tree) delete bone.second;
    tree->clear();
  }
}
