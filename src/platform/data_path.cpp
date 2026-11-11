// PC: where the game's files come from (see simmy/platform.hpp, "Game data").
//
// Normally a GameCube disc image of The Sims 2 (--iso, or iso= in simmy.ini): files are looked up in
// the disc's file table (cached when the image is opened) and read with positional reads of the one
// open image. For development, an extracted folder (--data) works the same way through the host file
// system. Both are read-only: nothing is ever written next to the game data.
#include "simmy/platform.hpp"
#include "gcm_disc.hpp"
#include "simmy/perf.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_thread.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace simmy {

// PC: the RAM copy of the game's archives (disc_cache, see data_source_start_cache below). One per
// cached file: its bytes, filled by a background thread in 4 MiB chunks, each chunk usable as soon as
// it is marked ready. Never freed (the process ends with _Exit).
struct CachedFile {
  std::string key;  // GcmDisc::normalize'd game path
  std::uint64_t size = 0;
  unsigned char* data = nullptr;
  std::unique_ptr<std::atomic<bool>[]> ready;  // per chunk
  std::size_t chunks = 0;
  std::atomic<bool> complete{false};
};

struct DataFile {
  const HostFile* host = nullptr;  // the disc image, or `owned` (a file of the data folder)
  std::unique_ptr<HostFile> owned;
  std::uint64_t base = 0;  // where the file starts in `host`
  std::uint64_t size = 0;
  std::uint64_t pos = 0;   // data_read / data_seek position
  std::string name;        // the game path (perf.hpp's disc read counts)
  CachedFile* cache = nullptr;  // its RAM copy (disc_cache), if it is one of the cached archives
  bool error = false;
  // The position and read-ahead are the game's EFile state: one thread uses a handle at a time, as on
  // the GC, but a stray concurrent use must not corrupt the buffer (std::FILE also locked per call).
  mutable std::mutex mutex;
  // Read-ahead for data_read: the game reads its archives in small pieces (headers, then each
  // resource), and a positional read per piece would be a system call each.
  std::vector<unsigned char> buf;
  std::uint64_t bufPos = 0;
  std::size_t bufLen = 0;
};

namespace {
constexpr std::size_t kReadAhead = 0x10000;

// Dev aid: SIMMY_IO_STATS=1 prints, when the game exits, how many files were opened and how many
// host reads (system calls) and bytes the game's reads took, and the time spent in them.
struct IoStats {
  bool on = std::getenv("SIMMY_IO_STATS") != nullptr;
  std::atomic<unsigned long long> opens{0}, reads{0}, bytes{0}, nanoseconds{0}, openNanoseconds{0};
  ~IoStats() {
    if (on)
      std::fprintf(stderr, "[io] %llu opens (%.1f ms), %llu host reads, %.1f MB, %.1f ms in reads\n", opens.load(),
                   static_cast<double>(openNanoseconds.load()) / 1e6, reads.load(),
                   static_cast<double>(bytes.load()) / 1048576.0, static_cast<double>(nanoseconds.load()) / 1e6);
  }
};
IoStats g_io;

constexpr std::uint64_t kCacheChunk = 4u << 20;
std::atomic<unsigned long long> g_cacheHits{0}, g_cacheHitBytes{0};

// From the RAM copy when every chunk the read touches is in (the same bytes as the image's).
bool CacheRead(const DataFile* f, std::uint64_t offset, void* dst, std::size_t size) {
  CachedFile* c = f->cache;
  if (!c || size == 0) return false;
  const std::uint64_t rel = offset - f->base;
  if (rel + size > c->size) return false;
  if (!c->complete.load(std::memory_order_acquire)) {
    for (std::uint64_t k = rel / kCacheChunk, last = (rel + size - 1) / kCacheChunk; k <= last; ++k)
      if (!c->ready[k].load(std::memory_order_acquire)) return false;
  }
  std::memcpy(dst, c->data + rel, size);
  g_cacheHits.fetch_add(1, std::memory_order_relaxed);
  g_cacheHitBytes.fetch_add(size, std::memory_order_relaxed);
  return true;
}

std::size_t HostRead(const DataFile* f, std::uint64_t offset, void* dst, std::size_t size, bool* ioError = nullptr) {
  if (CacheRead(f, offset, dst, size)) return size;
  const HostFile* host = f->host;
  const bool perfOn = perf::on();
  if (!g_io.on && !perfOn) return host->read_at(offset, dst, size, ioError);
  SIMMY_PERF_SCOPE(Disc);
  const auto t0 = std::chrono::steady_clock::now();
  const std::size_t n = host->read_at(offset, dst, size, ioError);
  const auto ns = static_cast<unsigned long long>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
  if (perfOn) perf::count_disc(f->name.c_str(), n, ns);
  if (!g_io.on) return n;
  g_io.nanoseconds += ns;
  ++g_io.reads;
  g_io.bytes += n;
  return n;
}

enum class Kind { None, Folder, Disc };
Kind g_kind = Kind::None;  // chosen once at startup, before any other thread runs
std::string g_folder;
std::unique_ptr<GcmDisc> g_disc;

// UTF-8 <-> std::filesystem (paths may hold any characters on Windows).
fs::path FromUtf8(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }
std::string ToUtf8(const fs::path& p) {
  const std::u8string u = p.u8string();
  return std::string(u.begin(), u.end());
}

bool IEquals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  return true;
}

// Folder mode: a game path ("data/index.ind", "/DATA\\Index.ind", ...) to a host path under the
// folder, matching case-insensitively like the disc. False if it does not exist.
bool ResolveFolderPath(const char* gamePath, std::string& out) {
  std::vector<std::string> parts;
  std::string cur;
  for (const char* p = gamePath; ; ++p) {
    if (*p == '/' || *p == '\\' || *p == '\0') {
      if (!cur.empty() && cur != ".") parts.push_back(cur);
      cur.clear();
      if (*p == '\0') break;
    } else {
      cur += *p;
    }
  }
  if (!parts.empty() && parts[0].size() >= 2 && parts[0].back() == ':') parts.erase(parts.begin());
  fs::path path = FromUtf8(g_folder);
  bool exists = true;
  for (const std::string& part : parts) {
    fs::path next = path / FromUtf8(part);
    std::error_code ec;
    if (exists && !fs::exists(next, ec)) {
      bool found = false;
      if (fs::is_directory(path, ec)) {
        for (const auto& entry : fs::directory_iterator(path, ec)) {
          if (IEquals(ToUtf8(entry.path().filename()), part)) {
            next = entry.path();
            found = true;
            break;
          }
        }
      }
      if (!found) exists = false;
    }
    path = next;
  }
  out = ToUtf8(path);
  return exists;
}

// The files the game cannot start without (it reads the archives that index.ind lists, the
// default neighbourhood and the memory-card icons).
const char* const kRequired[] = {"DATA/index.ind", "DATA/quickdat.arc", "DATA/textures.arc", "DATA/shaders.arc",
                                 "DATA/fonts.arc", "DATA/datasets.arc", "default.ngh"};
// Wanted, but the game runs without them (no movies / music / sound).
const char* const kOptional[] = {"DATA/movies.arc", "DATA/audiostr.arc", "DATA/samples.arc", "ngcicon.tpl", "ngcbanner.tpl"};

std::string MissingFiles(const char* const* names, size_t count) {
  std::string missing;
  for (size_t i = 0; i < count; ++i)
    if (!data_exists(names[i])) missing += (missing.empty() ? "" : ", ") + std::string(names[i]);
  return missing;
}
}  // namespace

bool data_source_open_disc(const std::string& path, std::string& error) {
  g_kind = Kind::None;
  g_disc = GcmDisc::open(path, error);
  if (!g_disc) return false;
  std::fprintf(stderr, "[disc] %s: game %s \"%s\" (version %u), %zu files, %.1f MB\n", path.c_str(),
               g_disc->game_id().c_str(), g_disc->title().c_str(), g_disc->disc_version(), g_disc->file_count(),
               static_cast<double>(g_disc->image().size()) / (1024.0 * 1024.0));
  g_kind = Kind::Disc;
  const std::string missing = MissingFiles(kRequired, sizeof kRequired / sizeof kRequired[0]);
  if (!missing.empty()) {
    error = "\"" + path + "\" (" + g_disc->game_id() + " \"" + g_disc->title() +
            "\") does not hold The Sims 2's game files (missing: " + missing +
            "). Choose a disc image of The Sims 2 for the Nintendo GameCube.";
    g_kind = Kind::None;
    g_disc.reset();
    return false;
  }
  // Any region's release will do as long as its files are there; just say when it is another game ID.
  if (g_disc->game_id().compare(0, 3, "G4Z") != 0)
    std::fprintf(stderr, "[disc] note: %s is not a game ID The Sims 2 is known by (G4Z?69); trying anyway\n",
                 g_disc->game_id().c_str());
  const std::string optional = MissingFiles(kOptional, sizeof kOptional / sizeof kOptional[0]);
  if (!optional.empty()) std::fprintf(stderr, "[disc] WARNING: not on the disc: %s\n", optional.c_str());
  return true;
}

bool data_source_open_folder(const std::string& dir, std::string& error) {
  std::error_code ec;
  if (!fs::is_directory(FromUtf8(dir), ec)) {
    error = "The game data folder \"" + dir + "\" does not exist.";
    return false;
  }
  g_folder = dir;
  g_kind = Kind::Folder;
  const std::string missing = MissingFiles(kRequired, sizeof kRequired / sizeof kRequired[0]);
  if (!missing.empty())  // as before: a warning only (tests may use partial folders)
    std::fprintf(stderr, "[file] WARNING: %s lacks %s\n", dir.c_str(), missing.c_str());
  std::fprintf(stderr, "[file] game data: folder %s\n", dir.c_str());
  return true;
}

bool data_source_ready() { return g_kind != Kind::None; }

std::string data_source_description() {
  switch (g_kind) {
  case Kind::Disc: return "disc image " + g_disc->game_id();
  case Kind::Folder: return "folder " + g_folder;
  default: return "no game data";
  }
}

static DataFile* OpenFile(const char* gamePath);
static DataFile* OpenFileForCache(const char* gamePath);

// ---- disc_cache: the game's archives in RAM ------------------------------------------------------
// The GameCube read its archives from the disc as it needed them, and the port did the same: every
// resource the game loads during play (a sound the first time a sim makes it, an animation, a texture,
// a model) was a synchronous read on the main thread, at a random place in a 1.4 GB image. From a hard
// disk or a cold OS cache that is a seek each (10-500 ms, measured in perf_log.txt as "disc"), which
// is the random hitching. With disc_cache on (default: auto = when the PC has 8 GB of RAM or more) a
// background thread reads the archives the game loads resources from, start to end in 4 MiB chunks,
// at low priority, into RAM (about 870 MB; movies.arc and audiostr.arc, streamed by their own threads,
// are left out); from then on the game's reads are memory copies. The bytes are the image's: the game
// sees no difference except time.
namespace {
// Smallest first (cheap, read early), then the big ones in the order in-play loads need them.
const char* const kCachedArchives[] = {
    "data/index.ind",    "data/index1.ind",   "data/levels.arc",   "data/ambients.arc", "data/effectsa.arc",
    "data/effectse.arc", "data/effectss.arc", "data/soundtra.arc", "data/fonts.arc",    "data/soundeve.arc",
    "data/binaries.arc", "data/characte.arc", "data/emitters.arc", "data/shaders.arc",  "data/edithtre.arc",
    "data/flashes.arc",  "data/quickdat.arc", "data/animatio.arc", "data/samples.arc",  "data/models.arc",
    "data/textures.arc", "data/datasets.arc"};
std::vector<std::unique_ptr<CachedFile>> g_cache;  // set up before any reader can see it
std::atomic<bool> g_cacheStarted{false};

CachedFile* FindCache(const char* gamePath) {
  if (g_cache.empty()) return nullptr;
  const std::string key = GcmDisc::normalize(gamePath);
  for (const auto& c : g_cache)
    if (c->key == key) return c.get();
  return nullptr;
}

bool g_cacheWarmOnly = false;  // disc_cache = warm: read through the OS file cache only, keep nothing

void CacheThread() {
#ifdef _WIN32
  // Low CPU and I/O priority: the game's own reads go first.
  SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#else
  SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
#endif
  const auto t0 = std::chrono::steady_clock::now();
  std::uint64_t total = 0;
  for (const auto& c : g_cache) {
    DataFile* f = OpenFileForCache(c->key.c_str());
    if (!f) continue;
    for (std::size_t k = 0; k < c->chunks; ++k) {
      const std::uint64_t at = k * kCacheChunk;
      const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(kCacheChunk, c->size - at));
      bool ioError = false;
      if (g_cacheWarmOnly) {
        static std::vector<unsigned char> scratch(kCacheChunk);
        if (f->host->read_at(f->base + at, scratch.data(), n, &ioError) != n) break;
        total += n;
        continue;
      }
      if (f->host->read_at(f->base + at, c->data + at, n, &ioError) != n) {
        std::fprintf(stderr, "[disc] cache: short read in %s at %llu: the rest of it is read from the disc\n",
                     c->key.c_str(), static_cast<unsigned long long>(at));
        break;
      }
      c->ready[k].store(true, std::memory_order_release);
      total += n;
    }
    bool all = !g_cacheWarmOnly;
    for (std::size_t k = 0; all && k < c->chunks; ++k) all = all && c->ready[k].load();
    c->complete.store(all, std::memory_order_release);
    delete f;
  }
  const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::fprintf(stderr, "[disc] cache: %.0f MB of game archives %s after %.1f s\n", static_cast<double>(total) / 1048576.0,
               g_cacheWarmOnly ? "read through the OS file cache" : "in RAM", s);
}
}  // namespace

void data_source_start_cache(int mode) {
  // mode: 0 off, 1 on (RAM), 2 auto (RAM with 8 GB of RAM or more, else warm), 3 warm
  if (g_cacheStarted.exchange(true) || g_kind == Kind::None) return;
  const int ramMb = SDL_GetSystemRAM();
  if (mode == 0) {
    std::fprintf(stderr, "[disc] cache: off (disc_cache = off)\n");
    return;
  }
  g_cacheWarmOnly = mode == 3 || (mode == 2 && ramMb > 0 && ramMb < 7600);
  std::uint64_t total = 0;
  for (const char* name : kCachedArchives) {
    DataFile* f = OpenFileForCache(name);
    if (!f) continue;
    auto c = std::make_unique<CachedFile>();
    c->key = GcmDisc::normalize(name);
    c->size = f->size;
    c->chunks = static_cast<std::size_t>((c->size + kCacheChunk - 1) / kCacheChunk);
    c->ready = std::make_unique<std::atomic<bool>[]>(c->chunks);
    for (std::size_t k = 0; k < c->chunks; ++k) c->ready[k] = false;
    if (!g_cacheWarmOnly) c->data = new (std::nothrow) unsigned char[c->size ? c->size : 1];
    delete f;
    if (!g_cacheWarmOnly && !c->data) break;
    total += c->size;
    g_cache.push_back(std::move(c));
  }
  std::fprintf(stderr, "[disc] cache: reading %zu archives (%.0f MB) %s in the background (%d MB of RAM)\n",
               g_cache.size(), static_cast<double>(total) / 1048576.0,
               g_cacheWarmOnly ? "through the OS file cache (warm: nothing kept)" : "into RAM", ramMb);
  std::thread(CacheThread).detach();
}

void data_cache_stats(unsigned long long& hits, unsigned long long& bytes) {
  hits = g_cacheHits.load(), bytes = g_cacheHitBytes.load();
}

DataFile* data_open(const char* gamePath) {
  if (!g_io.on) return OpenFile(gamePath);
  const auto t0 = std::chrono::steady_clock::now();
  DataFile* f = OpenFile(gamePath);
  g_io.openNanoseconds += static_cast<unsigned long long>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
  ++g_io.opens;
  return f;
}

static DataFile* OpenFile(const char* gamePath) {
  DataFile* f = OpenFileForCache(gamePath);
  if (f) f->cache = FindCache(gamePath);
  return f;
}

static DataFile* OpenFileForCache(const char* gamePath) {
  if (!gamePath) return nullptr;
  if (g_kind == Kind::Disc) {
    const GcmDisc::File* f = g_disc->find(gamePath);
    if (!f) return nullptr;
    auto* file = new DataFile();
    file->host = &g_disc->image();
    file->base = f->offset;
    file->size = f->size;
    file->name = gamePath;
    return file;
  }
  if (g_kind == Kind::Folder) {
    std::string path;
    if (!ResolveFolderPath(gamePath, path)) return nullptr;
    std::unique_ptr<HostFile> host = HostFile::open(path, nullptr);
    if (!host) return nullptr;
    auto* file = new DataFile();
    file->size = host->size();
    file->host = host.get();
    file->owned = std::move(host);
    file->name = gamePath;
    return file;
  }
  return nullptr;
}

void data_close(DataFile* f) { delete f; }

bool data_exists(const char* gamePath) {
  if (g_kind == Kind::Disc) return g_disc->find(gamePath) != nullptr;
  if (g_kind == Kind::Folder) {
    std::string path;
    std::error_code ec;
    return ResolveFolderPath(gamePath, path) && fs::is_regular_file(FromUtf8(path), ec);
  }
  return false;
}

std::uint64_t data_size(const DataFile* f) { return f ? f->size : 0; }
std::uint64_t data_tell(const DataFile* f) {
  if (!f) return 0;
  std::lock_guard lock(f->mutex);
  return f->pos;
}
void data_seek(DataFile* f, std::uint64_t pos) {
  if (!f) return;
  std::lock_guard lock(f->mutex);
  f->pos = pos;
}
bool data_error(const DataFile* f) {
  if (!f) return false;
  std::lock_guard lock(f->mutex);
  return f->error;
}

std::size_t data_read_at(const DataFile* f, std::uint64_t offset, void* dst, std::size_t size) {
  if (!f || offset >= f->size) return 0;
  const std::uint64_t left = f->size - offset;
  if (size > left) size = static_cast<std::size_t>(left);
  return HostRead(f, f->base + offset, dst, size);
}

std::size_t data_read(DataFile* f, void* dst, std::size_t size) {
  if (!f) return 0;
  std::lock_guard lock(f->mutex);
  if (f->pos >= f->size) return 0;
  const std::uint64_t left = f->size - f->pos;
  if (size > left) size = static_cast<std::size_t>(left);
  auto* out = static_cast<unsigned char*>(dst);
  std::size_t done = 0;
  while (done < size) {
    // From the read-ahead buffer.
    if (f->pos >= f->bufPos && f->pos < f->bufPos + f->bufLen) {
      const std::size_t at = static_cast<std::size_t>(f->pos - f->bufPos);
      const std::size_t n = std::min(f->bufLen - at, size - done);
      std::memcpy(out + done, f->buf.data() + at, n);
      done += n;
      f->pos += n;
      continue;
    }
    const std::size_t want = size - done;
    bool ioError = false;
    if (want >= kReadAhead) {  // large reads go straight into the caller's memory
      const std::size_t n = HostRead(f, f->base + f->pos, out + done, want, &ioError);
      done += n;
      f->pos += n;
      if (n < want) {
        f->error = true;  // short: the image or file is shorter than its table says
        break;
      }
      continue;
    }
    if (f->buf.empty()) f->buf.resize(kReadAhead);
    const std::size_t fill = static_cast<std::size_t>(std::min<std::uint64_t>(kReadAhead, f->size - f->pos));
    f->bufPos = f->pos;
    f->bufLen = HostRead(f, f->base + f->pos, f->buf.data(), fill, &ioError);
    if (f->bufLen == 0) {
      f->error = true;
      break;
    }
  }
  return done;
}

bool data_read_file(const char* gamePath, std::vector<unsigned char>& out) {
  DataFile* f = data_open(gamePath);
  if (!f) return false;
  out.resize(static_cast<std::size_t>(f->size));
  const bool ok = data_read_at(f, 0, out.data(), out.size()) == out.size();
  data_close(f);
  if (!ok) out.clear();
  return ok;
}

}  // namespace simmy
