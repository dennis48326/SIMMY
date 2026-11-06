// @class ERMovie after EResource EMovie
// A movie resource (engine/e_rmovie.cpp). It only remembers where the movie sits in its archive and
// owns the platform movie (EGraphics::CreateMovie) that plays it straight from there.
#pragma once

struct ERMovie : public EResource {
  ERMovie(EFile *file, unsigned int offset, unsigned int size, unsigned int id);  // 8031B4C8
  ~ERMovie() override;  // 8031B5A0
  void Start(int x, int y);  // 8031B62C
  void Stop();  // 8031B66C
  void Reset();  // 8031B6AC
  void Update();  // 8031B6EC
  int IsFinished();  // 8031B72C

  EFile* m_file = nullptr;     // +0x14 the archive
  unsigned int m_offset = 0;   // +0x18 position of the THP data in it
  unsigned int m_size = 0;     // +0x1C
  unsigned int m_movieId = 0;  // +0x20 EMovieMan::SetMovieId
  EMovie* m_movie = nullptr;   // +0x24 null when the movie failed to load
};
