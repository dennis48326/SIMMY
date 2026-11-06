// Port of game/bg.cpp (the completion flag) and the main menu's background call from game/thesims.cpp
// (BGExec/BGCall_SetDefaults). The other BGCall_* routines (neighbourhood, house, CAS) are not ported.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

int BG::s_bBGComplete = 1;  // 80500538 (set by bg.cpp's static initializer, 80014FF8)

// 80097FE0
void BGExec_SetDefaults() {
  _globals.SetDefaults();
  BG::s_bBGComplete = 1;
}

// 80098010
// Runs EGlobal::SetDefaults on the Background thread; BG::s_bBGComplete says when it is done.
void BGCall_SetDefaults() {
  BG::s_bBGComplete = 0;
  _pBackground->ExecuteRoutine(BGExec_SetDefaults);
}
