// PC: where the game's files come from (see simmy/platform.hpp, "Game data").
//
// Normally a GameCube disc image of The Sims 2 (--iso, or iso= in simmy.ini): files are looked up in
// the disc's file table (cached when the image is opened) and read with positional reads of the one
// open image. For development, an extracted folder (--data) works the same way through the host file
// system. Both are read-only: nothing is ever written next to the game data.
#include "simmy/platform.hpp"
#include "gcm_disc.hpp"

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
#include <vector>

namespace fs = std::filesystem;

namespace simmy {

struct DataFile {
  const HostFile* host = nullptr;  // the disc image, or `owned` (a file of the data folder)
  std::unique_ptr<HostFile> owned;
  std::uint64_t base = 0;  // where the file starts in `host`
  std::uint64_t size = 0;
  std::uint64_t pos = 0;   // data_read / data_seek position
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

std::size_t HostRead(const HostFile* host, std::uint64_t offset, void* dst, std::size_t size, bool* ioError = nullptr) {
  if (!g_io.on) return host->read_at(offset, dst, size, ioError);
  const auto t0 = std::chrono::steady_clock::now();
  const std::size_t n = host->read_at(offset, dst, size, ioError);
  g_io.nanoseconds += static_cast<unsigned long long>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
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
  if (!gamePath) return nullptr;
  if (g_kind == Kind::Disc) {
    const GcmDisc::File* f = g_disc->find(gamePath);
    if (!f) return nullptr;
    auto* file = new DataFile();
    file->host = &g_disc->image();
    file->base = f->offset;
    file->size = f->size;
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
  return HostRead(f->host, f->base + offset, dst, size);
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
      const std::size_t n = HostRead(f->host, f->base + f->pos, out + done, want, &ioError);
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
    f->bufLen = HostRead(f->host, f->base + f->pos, f->buf.data(), fill, &ioError);
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
