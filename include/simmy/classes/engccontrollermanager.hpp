// @class ENgcControllerManager after EControllerManager
// Reads the four GameCube pads through PAD (aurora on PC) once per frame.
#pragma once

struct ENgcControllerManager : public EControllerManager {
  ENgcControllerManager();
  ~ENgcControllerManager() override;
  int Init() override;
  void Shutdown() override;
  void Update() override;

  PADStatus m_pads[4] = {};  // 0x24
};
