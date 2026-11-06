// @class ENgcMovie after EMovie THPVideoInfo THPAudioInfo
// GameCube movie player (engine/e_ngcmovie.cpp): plays a THP movie stored inside DATA/MOVIES.ARC.
// Rewritten for PC on aurora's THP decoder: one host thread reads, decodes and times the frames (the
// original's reader and decoder threads and its VI-retrace PlayControl), and the renderer draws the
// current frame with the original's GX YUV->RGB TEV set-up (ENgcRenderer::MovieFrame).
// Video only: the THP audio track is skipped (audio is not ported).
#pragma once

struct ENgcMovie : public EMovie {
  ENgcMovie();  // 80335108
  ~ENgcMovie() override;  // 803351D8
  int Load(EFile *file, unsigned int offset, unsigned int size, unsigned int id) override;  // 80335244
  void Start(int x, int y) override;  // 803353A8
  void Stop() override;  // 80335460
  void Reset() override;  // 8033549C
  int IsFinished() override;  // 803354A0
  bool GetNextFrame();  // 803354FC
  RET_UNKNOWN PlayerInit(int audioSystem);  // 8033560C
  void PlayerQuit();  // 80335744
  RET_UNKNOWN PlayerOpen(unsigned int offset, bool onMemory);  // 803357A4
  static RET_UNKNOWN PlayerClose();  // 80335ABC
  RET_UNKNOWN PlayerCreateBuffers();  // 80335B0C (GameCube: took the EAllocGroup at +0x44)
  RET_UNKNOWN PlayerPrepare(int frame, int playFlag, int audioTrack);  // 80335CE4
  static RET_UNKNOWN PlayerPlay();  // 80335F10
  static void PlayerStop();  // 80335F6C
  static RET_UNKNOWN PlayerPause();  // 80336018
  static RET_UNKNOWN PlayerSkip();  // 80336054
  RET_UNKNOWN PlayerDrawCurrentFrame(GXRenderModeObj *, unsigned int x, unsigned int y, unsigned int w, unsigned int h);  // 803361FC
  RET_UNKNOWN PlayerGetVideoInfo(THPVideoInfo *);  // 803362B8
  RET_UNKNOWN PlayerGetAudioInfo(THPAudioInfo *);  // 803362F8
  static double PlayerGetFrameRate();  // 80336340
  static RET_UNKNOWN PlayerGetTotalFrame();  // 80336368
  static RET_UNKNOWN PlayerGetState();  // 80336388
  void PlayerDrawDone();  // 80336394

  int m_field0C = 0;                      // +0x0C cleared by Load
  int m_lastFrame = -1;                   // +0x10 frame number drawn last (GetNextFrame)
  unsigned int m_drawX = 0, m_drawY = 0;  // +0x18 top-left corner on screen
  int m_playerInitialized = 0;            // +0x20 PlayerInit succeeded
  GXRenderModeObj* m_renderMode = nullptr;  // +0x24 the renderer's render mode
  THPVideoInfo m_videoInfo;               // +0x28
  THPAudioInfo m_audioInfo;               // +0x34
  // +0x44 EAllocGroup for the player's buffers: on PC the player owns its buffers (std::vector).
  bool m_ownsPlayer = false;              // PC: Load opened the (single, global) player
};
