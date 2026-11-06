// Port of game/gameeffectsmanager.cpp: the game's side of the newer effects system. Before the level
// draws its order table, every effect that is drawn goes in (sorted at its client's point or its
// own position, or into the depth-of-field foreground list), and draws itself from there.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

GameEffectsManager* GameEffectsManager::s_pSingleton = nullptr;
EInstance EffectsPreRenderVisitor::s_DaveRemoveTheSuck;

// 8003DED4
// PC: the order table record's +0x18 (m_dl, typed EDL*) carries the effect, as on GC.
int EffectsPreRenderVisitor::Visit(Effects__FastParticleEmitter& e) {
  if (!(e.m_flags & Effects__Effect::kRender)) return 1;
  EOrderTableData otd;
  otd.m_flags = 0;
  otd.m_matrix = nullptr;
  otd.m_lights = nullptr;
  otd.m_animController = nullptr;
  otd.m_shader = nullptr;
  otd.m_pos = (e.m_sortByEmitter != 0 || !e.m_sortPos) ? &e.m_world.m[3][0] : e.m_sortPos;
  otd.m_sortKey = e.m_sortLayer;
  otd.m_instance = &s_DaveRemoveTheSuck;
  otd.m_callback = GameEffectsManager::RenderCB;
  auto* level = static_cast<ERLevel*>(_pEngine->m_currentLevel);
  if (!level) return 0;
  otd.m_dl = reinterpret_cast<EDL*>(static_cast<Effects__Effect*>(&e));
  if (!e.m_dofForeground) {
    level->InsertInOrderTable(otd);
  } else {
    otd.m_flags = 0x4000;
    level->InsertInDOFForegroundObjectList(otd);
  }
  return 1;
}

// 8003E008
GameEffectsManager::GameEffectsManager() {}

// 8003E00C
GameEffectsManager* GameEffectsManager::GetSingleton() {
  if (!s_pSingleton) s_pSingleton = new GameEffectsManager;
  return s_pSingleton;
}

// 8003E048
void GameEffectsManager::PreRender() {
  EffectsPreRenderVisitor v;
  Effects__EffectsManager::GetSingleton()->ForEachActiveEffect(v);
}

// 8003E094
// The effect draws; the level forgets its current shader (the effect selected its own).
void GameEffectsManager::RenderCB(ELevelDrawData& dd, EOrderTableData* otd) {
  auto* fx = reinterpret_cast<Effects__Effect*>(otd->m_dl);
  if (!fx) return;
  fx->Render(dd.m_rc);
  dd.m_curShader = nullptr;
}

// 8003E0EC __static_initialization_and_destruction_0 (s_DaveRemoveTheSuck)
// 8003E140
EffectsPreRenderVisitor::EffectsPreRenderVisitor() {}
// 8003E154
EffectsPreRenderVisitor::~EffectsPreRenderVisitor() {}
// 8003E188
int EffectsPreRenderVisitor::Visit(Effects__Effect&) { return 1; }
// 8003E190
int EffectsPreRenderVisitor::Visit(Effects__EffectsSequencer&) { return 1; }
// 8003E198 global constructors keyed to EffectsPreRenderVisitor::Visit
// 8003E1C4 global destructors keyed to EffectsPreRenderVisitor::Visit
