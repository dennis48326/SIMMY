// Port of engine/e_movieman.cpp: the "movies" resource manager.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 803251C8
// (+0x54, the archive's access mode, is 1 for movies: they are streamed from the archive.)
EMovieMan::EMovieMan() { m_archiveWritable = 1; }

// 80325058
// A movie is not read: the resource records where it starts in the archive and the platform player
// streams it from there. The archive is then left just past the movie, as for other resources.
EResource* EMovieMan::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  const int start = file->Tell();
  auto* movie = new ERMovie(file, static_cast<unsigned int>(start), size, m_movieId);
  movie->m_id = id;
  const int consumed = file->Tell() - start;
  file->Seek(static_cast<int>(size) - consumed, EFile__SeekType::Cur);
  return movie;
}

// 80325188
ERMovie* EMovieMan::AddRef(unsigned int id, EFile* file, int flags) {
  return static_cast<ERMovie*>(EResourceManager::AddRef(id, file, flags));
}

// 803251A8
ERMovie* EMovieMan::AddRef(char* name, EFile* file, int flags) {
  return static_cast<ERMovie*>(EResourceManager::AddRef(name, file, flags));
}

// 8032520C
void EMovieMan::SetMovieId(unsigned int id) { m_movieId = id; }
