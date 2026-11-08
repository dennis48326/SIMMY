// Port of game/castweaktool.cpp (the camera part): see castweaktool.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

CasTweakTool _gCasTweakTool;  // 80487870

// 801752A4
CasTweakTool::CasTweakTool() {}
// 801752CC
CasTweakTool::~CasTweakTool() {}

// 801753B0
void CasTweakTool::GetCamera(EVec3& eye, EVec3& target) {
  eye = m_eye;
  target = m_target;
}

// 801753EC
void CasTweakTool::SetCamera(EVec3& eye, EVec3& target) {
  m_eye = eye;
  m_target = target;
}
