// @class ENgcEngine
// engine/e_ngcengine.cpp: platform engine. Rewritten for PC (no ARAM/VM/OS arena setup).
#pragma once

struct ENgcEngine : public EEngine {
  ENgcEngine();
  virtual ~ENgcEngine();
  virtual int Init();
  static int InitConsole();
  static int InitMemoryManager();
  static void InitializeProfiler();
  virtual void PreFrameUpdate();
  void ErrorHandler(unsigned short, OSContext *, unsigned int, unsigned int);  // 8032E67C
  static int IsResetting();
  virtual void Idle();

  int m_field50 = 0;  // +0x50, cleared by the constructor; purpose unknown
};
