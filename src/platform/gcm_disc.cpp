// PC: GameCube disc image (GCM / plain .iso) reader, see gcm_disc.hpp.
#include "gcm_disc.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace simmy {

// ---- HostFile ----

#ifdef _WIN32
static std::wstring Widen(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
  if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
  return w;
}

static std::string LastErrorText() {
  const DWORD code = GetLastError();
  char buf[256] = "";
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, buf, sizeof buf, nullptr);
  std::string s = buf;
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '.')) s.pop_back();
  return s.empty() ? "error " + std::to_string(code) : s;
}

std::unique_ptr<HostFile> HostFile::open(const std::string& path, std::string* error) {
  const std::wstring wide = Widen(path);
  const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;  // e.g. open in Dolphin at the same time
  HANDLE h = CreateFileW(wide.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    if (error) *error = LastErrorText();
    return nullptr;
  }
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(h, &size)) {
    if (error) *error = LastErrorText();
    CloseHandle(h);
    return nullptr;
  }
  std::unique_ptr<HostFile> f(new HostFile());
  f->m_handle = h;
  f->m_size = static_cast<std::uint64_t>(size.QuadPart);
  return f;
}

HostFile::~HostFile() {
  if (m_handle) CloseHandle(static_cast<HANDLE>(m_handle));
}

std::size_t HostFile::read_at(std::uint64_t offset, void* dst, std::size_t size, bool* ioError) const {
  // A synchronous handle with an OVERLAPPED offset: each call names its own position, so concurrent
  // reads from several threads do not race on the file pointer.
  std::size_t done = 0;
  while (done < size) {
    const DWORD chunk = static_cast<DWORD>(size - done > 0x40000000u ? 0x40000000u : size - done);
    OVERLAPPED ov{};
    const std::uint64_t at = offset + done;
    ov.Offset = static_cast<DWORD>(at);
    ov.OffsetHigh = static_cast<DWORD>(at >> 32);
    DWORD got = 0;
    if (!ReadFile(static_cast<HANDLE>(m_handle), static_cast<char*>(dst) + done, chunk, &got, &ov)) {
      if (GetLastError() != ERROR_HANDLE_EOF && ioError) *ioError = true;
      break;
    }
    if (got == 0) break;
    done += got;
  }
  return done;
}
#else
std::unique_ptr<HostFile> HostFile::open(const std::string& path, std::string* error) {
  int flags = O_RDONLY;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
  const int fd = ::open(path.c_str(), flags);
  if (fd < 0) {
    if (error) *error = std::strerror(errno);
    return nullptr;
  }
  struct stat st{};
  if (fstat(fd, &st) != 0 || S_ISDIR(st.st_mode)) {
    if (error) *error = S_ISDIR(st.st_mode) ? "it is a folder, not a file" : std::strerror(errno);
    ::close(fd);
    return nullptr;
  }
  std::unique_ptr<HostFile> f(new HostFile());
  f->m_fd = fd;
  f->m_size = static_cast<std::uint64_t>(st.st_size);
  return f;
}

HostFile::~HostFile() {
  if (m_fd >= 0) ::close(m_fd);
}

std::size_t HostFile::read_at(std::uint64_t offset, void* dst, std::size_t size, bool* ioError) const {
  std::size_t done = 0;
  while (done < size) {
    const ssize_t n = ::pread(m_fd, static_cast<char*>(dst) + done, size - done, static_cast<off_t>(offset + done));
    if (n < 0) {
      if (errno == EINTR) continue;
      if (ioError) *ioError = true;
      break;
    }
    if (n == 0) break;
    done += static_cast<std::size_t>(n);
  }
  return done;
}
#endif

// ---- GcmDisc ----

namespace {
std::uint32_t BE32(const unsigned char* p) {
  return static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 |
         static_cast<std::uint32_t>(p[2]) << 8 | p[3];
}

#define SIMMY_DOLPHIN_CONVERT                                                                          \
  "in Dolphin, right-click the game, choose \"Convert File...\", set Format to \"ISO\" and click Convert, " \
  "then choose the new .iso file."
const char kConvert[] = "Please convert it to a plain .iso first: " SIMMY_DOLPHIN_CONVERT;
const char kConvertLower[] = "convert it to a plain .iso: " SIMMY_DOLPHIN_CONVERT;

std::string Quote(const std::string& path) { return "\"" + path + "\""; }

std::string Printable(const unsigned char* p, std::size_t max) {
  std::string s;
  for (std::size_t i = 0; i < max && p[i]; ++i) s += (p[i] >= 0x20 && p[i] < 0x7F) ? static_cast<char>(p[i]) : '?';
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}
}  // namespace

std::string GcmDisc::normalize(const char* path) {
  std::string out, part;
  auto flush = [&] {
    if (!part.empty() && part != ".") {
      if (!out.empty()) out += '/';
      out += part;
    }
    part.clear();
  };
  bool first = true;
  for (const char* p = path ? path : ""; ; ++p) {
    if (*p == '/' || *p == '\\' || *p == '\0') {
      if (first && part.size() >= 2 && part.back() == ':') part.clear();  // "dvd:" and the like
      first = false;
      flush();
      if (*p == '\0') break;
    } else {
      const char c = *p;
      part += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
  }
  return out;
}

const GcmDisc::File* GcmDisc::find(const char* path) const {
  const auto it = m_files.find(normalize(path));
  return it == m_files.end() ? nullptr : &it->second;
}

bool GcmDisc::is_directory(const char* path) const {
  const std::string key = normalize(path);
  return key.empty() || m_dirs.count(key) != 0;
}

std::unique_ptr<GcmDisc> GcmDisc::open(const std::string& path, std::string& error) {
  std::error_code ec;
  if (std::filesystem::is_directory(std::filesystem::path(std::u8string(path.begin(), path.end())), ec)) {
    error = Quote(path) + " is a folder. Choose the disc image file itself (a .iso or .gcm file).";
    return nullptr;
  }
  std::string why;
  std::unique_ptr<HostFile> image = HostFile::open(path, &why);
  if (!image) {
    error = "Cannot open the disc image " + Quote(path) + ": " + why + ".";
    return nullptr;
  }
  const std::uint64_t imageSize = image->size();
  unsigned char boot[0x440] = {};
  const std::size_t got = image->read_at(0, boot, sizeof boot);

  // Other container formats first (they do not have a GameCube header at 0).
  if (got >= 4) {
    if (std::memcmp(boot, "RVZ\x01", 4) == 0 || std::memcmp(boot, "WIA\x01", 4) == 0) {
      error = Quote(path) + " is a compressed Dolphin image (" + std::string(std::memcmp(boot, "RVZ", 3) == 0 ? "RVZ" : "WIA") +
              "). SIMMY reads only plain GameCube disc images (.iso / .gcm). " + kConvert;
      return nullptr;
    }
    if (std::memcmp(boot, "CISO", 4) == 0) {
      error = Quote(path) + " is a compressed CISO image. SIMMY reads only plain GameCube disc images (.iso / .gcm). " +
              kConvert;
      return nullptr;
    }
    if (boot[0] == 0x01 && boot[1] == 0xC0 && boot[2] == 0x0B && boot[3] == 0xB1) {  // 0xB10BC001, little-endian
      error = Quote(path) + " is a compressed GCZ image. SIMMY reads only plain GameCube disc images (.iso / .gcm). " +
              kConvert;
      return nullptr;
    }
    if (std::memcmp(boot, "WBFS", 4) == 0) {
      error = Quote(path) + " is a WBFS (Wii) image, not a GameCube disc. SIMMY needs The Sims 2 for GameCube as a plain .iso.";
      return nullptr;
    }
    if (BE32(boot) == 0xAE0F38A2) {
      error = Quote(path) + " is a TGC image (a GameCube demo-disc container). " + kConvert;
      return nullptr;
    }
  }
  if (got < sizeof boot) {
    error = Quote(path) + " is too small to be a GameCube disc image (" + std::to_string(imageSize) +
            " bytes). It may be damaged or incomplete; copy it again from the original.";
    return nullptr;
  }
  if (BE32(boot + 0x18) == 0x5D1C9EA3) {
    error = Quote(path) + " is a Wii disc image (" + Printable(boot, 6) + " \"" + Printable(boot + 0x20, 0x60) +
            "\"). SIMMY needs The Sims 2 for the Nintendo GameCube.";
    return nullptr;
  }
  if (BE32(boot + 0x1C) != 0xC2339F3D) {
    error = Quote(path) +
            " is not a GameCube disc image (no GameCube disc header). Choose a plain .iso / .gcm of "
            "The Sims 2 for GameCube. If it is a compressed image (.rvz, .wia, .gcz, .ciso, .nkit), " + kConvertLower;
    return nullptr;
  }
  unsigned char nkit[4] = {};
  if (image->read_at(0x200, nkit, 4) == 4 && std::memcmp(nkit, "NKIT", 4) == 0) {
    error = Quote(path) +
            " is an NKit image, which leaves out or shrinks parts of the disc. Restore it to a full disc "
            "image with NKit (\"Recover to ISO\"), or " + std::string(kConvertLower);
    return nullptr;
  }

  std::unique_ptr<GcmDisc> disc(new GcmDisc());
  disc->m_gameId = Printable(boot, 6);
  disc->m_version = boot[7];
  disc->m_title = Printable(boot + 0x20, 0x3E0);
  const std::uint32_t fstOffset = BE32(boot + 0x424), fstSize = BE32(boot + 0x428);
  const std::string named = Quote(path) + " (" + disc->m_gameId + " \"" + disc->m_title + "\")";
  const char* const redump = " The image is damaged or incomplete; copy or dump it again from the original disc.";
  if (fstSize < 12 || fstSize > 0x4000000u || static_cast<std::uint64_t>(fstOffset) + fstSize > imageSize) {
    error = named + ": its file table lies outside the image (the image is " + std::to_string(imageSize) + " bytes)." + redump;
    return nullptr;
  }
  std::vector<unsigned char> fst(fstSize);
  if (image->read_at(fstOffset, fst.data(), fst.size()) != fst.size()) {
    error = named + ": cannot read its file table." + redump;
    return nullptr;
  }
  const std::uint32_t count = BE32(fst.data() + 8);
  if (fst[0] != 1 || count == 0 || static_cast<std::uint64_t>(count) * 12 > fstSize) {
    error = named + ": its file table is not valid." + redump;
    return nullptr;
  }
  const std::uint32_t stringsAt = count * 12;
  struct Dir {
    std::uint32_t end;
    std::string prefix;
  };
  std::vector<Dir> stack{{count, std::string()}};
  std::uint64_t furthest = 0;
  for (std::uint32_t i = 1; i < count; ++i) {
    while (stack.size() > 1 && i >= stack.back().end) stack.pop_back();
    const unsigned char* e = fst.data() + i * 12;
    const std::uint32_t nameOff = BE32(e) & 0xFFFFFFu;
    if (static_cast<std::uint64_t>(stringsAt) + nameOff >= fstSize) {
      error = named + ": its file table is not valid (bad name)." + redump;
      return nullptr;
    }
    const char* name = reinterpret_cast<const char*>(fst.data() + stringsAt + nameOff);
    const std::size_t nameLen = strnlen(name, fstSize - stringsAt - nameOff);
    const std::string full = stack.back().prefix + GcmDisc::normalize(std::string(name, nameLen).c_str());
    if (e[0] & 1) {
      const std::uint32_t next = BE32(e + 8);
      if (next <= i || next > stack.back().end) {
        error = named + ": its file table is not valid (bad folder)." + redump;
        return nullptr;
      }
      disc->m_dirs[full] = true;
      stack.push_back({next, full + "/"});
    } else {
      File f{BE32(e + 4), BE32(e + 8)};
      const std::uint64_t end = static_cast<std::uint64_t>(f.offset) + f.size;
      if (end > furthest) furthest = end;
      disc->m_files[full] = f;
    }
  }
  if (furthest > imageSize) {
    error = named + " is incomplete: it is " + std::to_string(imageSize) + " bytes, but its files reach up to byte " +
            std::to_string(furthest) + "." + redump;
    return nullptr;
  }
  disc->m_image = std::move(image);
  return disc;
}

}  // namespace simmy
