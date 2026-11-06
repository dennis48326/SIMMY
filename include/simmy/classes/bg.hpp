// @class BG
// The background loader's completion flag (game/bg.cpp): set when the routine a BGCall_* handed to the
// Background thread has finished.
#pragma once

struct BG {
  static int s_bBGComplete;  // 80500538 (1 at start)
};
