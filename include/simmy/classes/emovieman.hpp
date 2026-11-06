// @class EMovieMan after EResourceManager
// The "movies" resource manager (engine/e_movieman.cpp). A movie resource is not read into memory:
// ERMovie records its archive position and the platform player streams it from the archive.
#pragma once

struct EMovieMan : public EResourceManager {
  EMovieMan();  // 803251C8
  EResource* AllocateAndLoadResource(EFile *file, unsigned int size, unsigned int id) override;  // 80325058
  ERMovie * AddRef(unsigned int id, EFile *file, int flags);  // 80325188
  ERMovie * AddRef(char *name, EFile *file, int flags);  // 803251A8
  void SetMovieId(unsigned int id);  // 8032520C

  unsigned int m_movieId = 0;  // +0xD20 passed to each movie's Load
};
