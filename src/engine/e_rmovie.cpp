// Port of engine/e_rmovie.cpp: the movie resource, a thin owner of the platform movie.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 8031B4C8
// The platform movie opens the THP data where it sits in the archive; if that fails the resource
// stays without a movie and reports itself finished.
ERMovie::ERMovie(EFile* file, unsigned int offset, unsigned int size, unsigned int id)
    : m_file(file), m_offset(offset), m_size(size), m_movieId(id) {
  m_movie = _pGfx->CreateMovie();
  if (m_movie && !m_movie->Load(file, offset, size, id)) {
    _pGfx->Destroy(m_movie);
    m_movie = nullptr;
  }
}

// 8031B5A0
ERMovie::~ERMovie() {
  if (m_movie) {
    _pGfx->Destroy(m_movie);
    m_movie = nullptr;
  }
}

// 8031B62C
void ERMovie::Start(int x, int y) {
  if (m_movie) m_movie->Start(x, y);
}

// 8031B66C
void ERMovie::Stop() {
  if (m_movie) m_movie->Stop();
}

// 8031B6AC
void ERMovie::Reset() {
  if (m_movie) m_movie->Reset();
}

// 8031B6EC
void ERMovie::Update() {
  if (m_movie) m_movie->Update();
}

// 8031B72C
int ERMovie::IsFinished() {
  if (m_movie && !m_movie->IsFinished()) return 0;
  return 1;
}
