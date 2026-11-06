// @class ENgcRendCommand after EEvent EDL
// A renderer command. The GameCube queued these to the render thread; Queue* now builds one on the
// stack and runs it immediately.
#pragma once

struct ENgcRendCommand {
  enum Type { kBeginFrame = 1, kDisplayList = 2, kEndFrame = 3, kSwap = 4 };
  int m_type = 0;
  int m_arg0 = 0;        // BeginFrame: buffer; Swap: even/odd
  int m_arg1 = 0;        // Swap: minimum retraces
  EEvent* m_done = nullptr;  // EndFrame: released once the frame is out
  EDL* m_dl = nullptr;       // DisplayList: the list to run
  bool m_release = false;    // DisplayList: free it afterwards
};
