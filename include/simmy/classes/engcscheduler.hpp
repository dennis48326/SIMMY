// @class ENgcScheduler
// Render scheduler (engine/e_ngcscheduler.cpp). On the GameCube a thread recycled command blocks;
// every Queue* call already forwarded straight to the renderer, so on PC it needs no thread.
#pragma once

struct ENgcScheduler : public EThread {
  ENgcScheduler();  // 8034D7C4
  virtual ~ENgcScheduler();  // 8034D828
  virtual RET_UNKNOWN Init();  // 8034D89C
  virtual void QueueSetupFrameBuffer(int);  // 8034D918
  virtual void QueueDisplayList(EDL *, bool);  // 8034D940
  virtual void QueueCompletionEvent(EEvent &);  // 8034D968
  virtual void QueueSwapBuffer(int, int);  // 8034D990
  virtual void Flush();  // 8034D9B8
  virtual void TextureLoadsComplete(ESchedCommand *);  // 8034D9E0
  virtual void RenderingComplete(ESchedCommand *);  // 8034D9E4
  virtual RET_UNKNOWN GetLastRetraceCount();  // 8034D9E8
  virtual void Main();  // 8034D9F0
  void SendCommand(ESchedCommand *);  // 8034DA28
  RET_UNKNOWN AllocSchedCommand();  // 8034DA50
  void FreeSchedCommand(ESchedCommand *);  // 8034DADC
  virtual void RendFrameComplete(ESchedCommand *);  // 8034DBA8
};
