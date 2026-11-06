// PC: reading the game straight from a GameCube disc image (a plain .iso / .gcm, "GCM" layout).
//
// A GCM image is the disc's bytes as they are: boot.bin (0x440 bytes: game ID at 0x0, magic
// 0xC2339F3D at 0x1C, title at 0x20, FST offset/size at 0x424/0x428), bi2.bin, the apploader, the
// DOL and the FST (file system table: 12-byte entries, then the name strings). Every file is one
// contiguous run of bytes in the image, so a file read is a positional read of the image.
// Compressed images (RVZ, WIA, GCZ, CISO, WBFS, NKit) and Wii discs are recognised and rejected with
// a message saying how to convert them.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace simmy {

// A read-only host file with positional reads (pread / ReadFile at an offset): no shared file
// position, so any number of threads may read one HostFile at once. Paths are UTF-8.
class HostFile {
public:
  static std::unique_ptr<HostFile> open(const std::string& path, std::string* error);
  ~HostFile();
  HostFile(const HostFile&) = delete;
  HostFile& operator=(const HostFile&) = delete;
  std::uint64_t size() const { return m_size; }
  // Reads up to `size` bytes at `offset`; returns the count read (short at the end of the file).
  // `ioError` (optional) is set when the OS reported an error.
  std::size_t read_at(std::uint64_t offset, void* dst, std::size_t size, bool* ioError = nullptr) const;

private:
  HostFile() = default;
#ifdef _WIN32
  void* m_handle = nullptr;  // HANDLE
#else
  int m_fd = -1;
#endif
  std::uint64_t m_size = 0;
};

class GcmDisc {
public:
  struct File {
    std::uint32_t offset = 0;  // in the image
    std::uint32_t size = 0;
  };
  // Opens and checks the image. On failure returns null and sets `error` to a message for the user.
  static std::unique_ptr<GcmDisc> open(const std::string& path, std::string& error);

  // `path` as the game names it: case-insensitive, '/' or '\', leading '/', "./" or "dvd:" allowed.
  const File* find(const char* path) const;
  bool is_directory(const char* path) const;
  const HostFile& image() const { return *m_image; }
  const std::string& game_id() const { return m_gameId; }  // e.g. "G4ZE69"
  const std::string& title() const { return m_title; }
  unsigned disc_version() const { return m_version; }
  std::size_t file_count() const { return m_files.size(); }

  // The canonical lookup key for a game path (lower case, '/' separated, no leading '/').
  static std::string normalize(const char* path);

private:
  std::unique_ptr<HostFile> m_image;
  std::unordered_map<std::string, File> m_files;  // the FST, cached: normalized path -> extent
  std::unordered_map<std::string, bool> m_dirs;
  std::string m_gameId, m_title;
  unsigned m_version = 0;
};

}  // namespace simmy
