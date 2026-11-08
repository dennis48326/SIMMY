// @class CasTweakTool after EVec3 ELights
// CAS's tweak tool (game/castweaktool.cpp, `_gCasTweakTool` 80487870, 0x114 bytes): on GC a dev tool's
// store of the CAS lights and camera. The release game still goes through it: CasScene's camera moves
// store their destination in it and, at rest with the C-stick released, the camera reads back the last
// stored spot (so a zoom stays where the stick left it).
#pragma once

struct CasTweakTool {
  CasTweakTool();  // 801752A4
  ~CasTweakTool();  // 801752CC
  void GetLights(ELights&);  // 80175300 (not ported: only CasScene::SetSceneLighting's tweak mode reads it)
  void SetLights(ELights&);  // 80175358
  void GetCamera(EVec3& eye, EVec3& target);  // 801753B0
  void SetCamera(EVec3& eye, EVec3& target);  // 801753EC

  // +0x00..+0xF7: the lights (not ported)
  EVec3 m_eye;     // +0xF8
  EVec3 m_target;  // +0x104
  // +0x110: the vtable
};

extern CasTweakTool _gCasTweakTool;  // 80487870
