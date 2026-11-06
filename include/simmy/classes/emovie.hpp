// @class EMovie
// Platform-independent movie interface (engine/e_movie.cpp). The platform movie (ENgcMovie) overrides
// everything but Update, which draws the current frame through an immediate render context.
#pragma once

struct EMovie {
  EMovie();  // 80364D24
  virtual void Update();  // 80364C24
  virtual int Load(EFile *, unsigned int, unsigned int, unsigned int);  // 80364CCC
  virtual void Start(int, int);  // 80364CD4
  virtual void Stop();  // 80364CE0
  virtual void Reset();  // 80364CE4
  virtual int IsFinished();  // 80364CE8
  virtual ~EMovie();  // 80364CF0

  int m_x = 0;  // +0x00 where the movie is drawn (screen pixels)
  int m_y = 0;  // +0x04
  // +0x08 vtable on the GameCube
};
