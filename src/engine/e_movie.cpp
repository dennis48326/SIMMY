// Port of engine/e_movie.cpp: the platform-independent movie base class.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80364D24
EMovie::EMovie() {}

// 80364CF0
EMovie::~EMovie() = default;

// 80364C24
// While the movie plays, each frame asks the renderer (through an immediate render context) to draw
// the movie's current frame.
void EMovie::Update() {
  if (IsFinished()) return;
  ERC* rc = _pGfx->Open(RCMode::Immediate);
  rc->MovieFrame(this);
  _pGfx->Close(rc);
}

// 80364CCC
int EMovie::Load(EFile*, unsigned int, unsigned int, unsigned int) { return 0; }

// 80364CD4
void EMovie::Start(int x, int y) {
  m_y = y;
  m_x = x;
}

// 80364CE0
void EMovie::Stop() {}

// 80364CE4
void EMovie::Reset() {}

// 80364CE8
int EMovie::IsFinished() { return 1; }
