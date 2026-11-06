// Port of engine/effectsinterface.cpp and engine/effectsmanager.cpp (the newer effects system's
// start-up and its manager). The manager's Update also renews the random table both particle
// systems draw from (Effects::PreComputeRand32Table).
//
// Effects are only made from effects emitters (FastParticleEmitter): the GC's CreateEffect for
// sequencers returns nothing.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>

namespace {
int l_bIsInitialized = 0;
}  // namespace

Effects__EffectsManager* Effects__EffectsManager::s_pSingleton = nullptr;

// 80352258
int Effects::InitializeEffectsSystem() {
  if (l_bIsInitialized) return 0;
  l_bIsInitialized = 1;
  InitEffectsMath();
  Effects__FastParticleEmitter::InitModule();
  return Effects__EffectsManager::GetSingleton()->Initialize();
}

// 803522A0
bool Effects::ShutdownEffectsSystem() {
  const bool was = l_bIsInitialized != 0;
  if (was) {
    l_bIsInitialized = 0;
    Effects__EffectsManager::GetSingleton();
    Effects__EffectsManager::Shutdown();
  }
  return was;
}

// 80352A10
void Effects::InitEffectsMath() {}

// 80353090
void Effects__FastParticleEmitter::InitPlatform() {}
// 80353094
void Effects__FastParticleEmitter::InitModule() {
  InitParticlePool();
  Effects::SetSeed2(static_cast<int>(0xdecafbad));
  InitPlatform();
}

// 803522E4
Effects__EffectsManager::Effects__EffectsManager() {}
// 80352318
Effects__EffectsManager::~Effects__EffectsManager() {
  for (EffectEntry* e : m_deleted) delete e;
  for (EffectEntry* e : m_active) delete e;
}

// 80352364
Effects__EffectsManager* Effects__EffectsManager::GetSingleton() {
  if (!s_pSingleton) s_pSingleton = new Effects__EffectsManager;
  return s_pSingleton;
}

// 803523A0
int Effects__EffectsManager::Initialize() {
  if (m_initialized) return 0;
  m_initialized = 1;
  return 1;
}

// 803523C8
int Effects__EffectsManager::Shutdown() { return 1; }

// 803523D0
// Once a frame: a new random table; each active effect that is updated runs a frame, or once it is
// to be deleted moves to the deleted list, where it goes three frames later (the order table may
// still point at it until then).
void Effects__EffectsManager::Update(float dt) {
  if (!m_initialized) return;
  Effects::PreComputeRand32Table();
  for (size_t i = 0; i < m_active.size();) {
    EffectEntry* e = m_active[i];
    Effects__Effect* fx = e->m_effect;
    if (fx->m_flags & Effects__Effect::kUpdate) {
      if (!(fx->m_flags & Effects__Effect::kDelete)) {
        fx->Update(dt);
      } else {
        size_t at = i;
        RemoveEntryFromList(e, m_active, &at);
        ScheduleEffectForDeletion(e);
        continue;
      }
    }
    ++i;
  }
  for (size_t i = 0; i < m_deleted.size();) {
    EffectEntry* e = m_deleted[i];
    if (++e->m_framesDeleted == 3) {
      delete e->m_effect;
      delete e;
      m_deleted.erase(m_deleted.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

// 80352534
void Effects__EffectsManager::Render(ERC* rc) {
  if (!m_initialized) return;
  for (EffectEntry* e : m_active)
    if (e->m_effect->m_flags & Effects__Effect::kRender) e->m_effect->Render(rc);
}

// 803525C0
int Effects__EffectsManager::DeleteAll() {
  while (!m_active.empty()) {
    EffectEntry* e = m_active.front();
    size_t at = 0;
    RemoveEntryFromList(e, m_active, &at);
    ScheduleEffectForDeletion(e);
  }
  return 1;
}

// 80352634
// A FastParticleEmitter running `res` (with the client's matrix and sort settings).
Effects__Effect* Effects__EffectsManager::CreateEffect(REffectsEmitter& res, Effects__Effect__ClientParams* params) {
  if (!m_initialized) return nullptr;
  auto* fx = params ? new Effects__FastParticleEmitter(*params) : new Effects__FastParticleEmitter;
  if (fx->Init(res)) {
    AddEffect(fx);
    return fx;
  }
  delete fx;
  return nullptr;
}

// 80352710
Effects__Effect* Effects__EffectsManager::CreateEffect(REffectsSequencer&, Effects__Effect__ClientParams*) { return nullptr; }

// 80352718
int Effects__EffectsManager::DeleteEffect(Effects__Effect& effect) {
  if (!m_initialized) return 0;
  size_t at = 0;
  if (EffectEntry* e = FindEffectInList(&effect, m_active, &at)) {
    RemoveEntryFromList(e, m_active, &at);
    ScheduleEffectForDeletion(e);
  }
  return 1;
}

// 80352798
void Effects__EffectsManager::ScheduleEffectForDeletion(EffectEntry* e) {
  if (e->m_deleted) return;
  e->m_deleted = 1;
  AddEntryToList(e, m_deleted);
}

// 803527D0
int Effects__EffectsManager::AddEffect(Effects__Effect* effect) {
  auto* e = new EffectEntry;
  e->m_effect = effect;
  const bool ok = AddEntryToList(e, m_active);
  if (!ok) {
    delete effect;
    delete e;
  }
  return ok;
}

// 80352860
bool Effects__EffectsManager::AddEntryToList(EffectEntry* e, List& list) {
  if (e) list.push_back(e);
  return e != nullptr;
}

// 80352894
// (`at`: where it is, if known.)
int Effects__EffectsManager::RemoveEntryFromList(EffectEntry* e, List& list, size_t* at) {
  if (!e) return 0;
  auto it = at && *at < list.size() && list[*at] == e ? list.begin() + static_cast<std::ptrdiff_t>(*at)
                                                      : std::find(list.begin(), list.end(), e);
  if (it == list.end()) return 0;
  list.erase(it);
  return 1;
}

// 80352910
Effects__EffectsManager::EffectEntry* Effects__EffectsManager::FindEffectInList(Effects__Effect* effect, List& list, size_t* at) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i]->m_effect != effect) continue;
    if (at) *at = i;
    return list[i];
  }
  return nullptr;
}

// 80352950
int Effects__EffectsManager::ForEachActiveEffect(Effects__EffectsVisitor& v) { return ForEachEffectInList(v, m_active); }
// 80352974
int Effects__EffectsManager::ForEachDeletedEffect(Effects__EffectsVisitor& v) { return ForEachEffectInList(v, m_deleted); }

// 80352998
int Effects__EffectsManager::ForEachEffectInList(Effects__EffectsVisitor& v, List& list) {
  for (EffectEntry* e : list)
    if (!e->m_effect->Accept(v)) return 0;
  return 1;
}
