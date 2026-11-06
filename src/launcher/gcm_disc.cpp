// SimmyLauncher: GameCube disc image checks and banner (see gcm_disc.hpp).
#include "gcm_disc.hpp"

#include <SDL3/SDL_iostream.h>

#include <cstring>

namespace launcher {

namespace {

uint32_t BE32(const unsigned char* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

struct Reader {
  SDL_IOStream* io = nullptr;
  uint64_t size = 0;
  ~Reader() {
    if (io) SDL_CloseIO(io);
  }
  bool read(uint64_t offset, void* dst, size_t bytes) {
    if (!io || offset > size || bytes > size - offset) return false;
    if (SDL_SeekIO(io, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) < 0) return false;
    return SDL_ReadIO(io, dst, bytes) == bytes;
  }
};

// Latin-1 (the banners of American and European discs) to UTF-8, stopping at the first NUL. Japanese
// discs use Shift-JIS, which the launcher's font cannot show: only their ASCII part is kept.
std::string Latin1ToUtf8(const unsigned char* p, size_t max, bool asciiOnly) {
  std::string out;
  for (size_t i = 0; i < max && p[i]; ++i) {
    const unsigned char c = p[i];
    if (c == '\n' || c == '\r' || c == '\t') {
      out += ' ';
    } else if (c < 0x20 || c == 0x7F) {
      continue;
    } else if (c < 0x80) {
      out += static_cast<char>(c);
    } else if (asciiOnly || c < 0xA0) {
      continue;
    } else {
      out += static_cast<char>(0xC0 | (c >> 6));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
  }
  // Trim spaces.
  size_t a = 0, b = out.size();
  while (a < b && out[a] == ' ') ++a;
  while (b > a && out[b - 1] == ' ') --b;
  return out.substr(a, b - a);
}

DiscInfo Fail(DiscInfo info, DiscStatus status, const std::string& message, const char* format = "") {
  info.status = status;
  info.message = message;
  info.format = format;
  return info;
}

}  // namespace

void decode_rgb5a3_banner(const unsigned char* src, std::vector<uint32_t>& out) {
  out.assign(static_cast<size_t>(kBannerWidth) * kBannerHeight, 0);
  size_t at = 0;
  for (int ty = 0; ty < kBannerHeight; ty += 4)
    for (int tx = 0; tx < kBannerWidth; tx += 4)
      for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x, at += 2) {
          const unsigned v = (unsigned(src[at]) << 8) | src[at + 1];
          unsigned r, g, b, a;
          if (v & 0x8000) {  // RGB555, opaque
            r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
            r = (r << 3) | (r >> 2), g = (g << 3) | (g >> 2), b = (b << 3) | (b >> 2);
            a = 255;
          } else {  // ARGB3444
            a = (v >> 12) & 7, r = (v >> 8) & 15, g = (v >> 4) & 15, b = v & 15;
            r *= 17, g *= 17, b *= 17;
            a = (a << 5) | (a << 2) | (a >> 1);
          }
          const unsigned char px[4] = {static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                                       static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
          uint32_t word;
          std::memcpy(&word, px, 4);
          out[static_cast<size_t>(ty + y) * kBannerWidth + tx + x] = word;
        }
}

DiscInfo probe_disc(const std::string& path) {
  DiscInfo info;
  info.path = path;
  Reader rd;
  rd.io = SDL_IOFromFile(path.c_str(), "rb");
  if (!rd.io) return Fail(info, DiscStatus::Missing, "SIMMY can't open this file. Is it still there?");
  const Sint64 size = SDL_GetIOSize(rd.io);
  rd.size = size > 0 ? static_cast<uint64_t>(size) : 0;
  info.fileSize = rd.size;

  unsigned char head[0x440] = {};
  const size_t headBytes = rd.size < sizeof head ? static_cast<size_t>(rd.size) : sizeof head;
  if (headBytes < 8 || !rd.read(0, head, headBytes))
    return Fail(info, DiscStatus::NotADisc, "This file is too small to be a GameCube disc.");

  // Compressed and container formats the game cannot read.
  const uint32_t magic = BE32(head);
  struct Container {
    const char* tag;
    const char* name;
  };
  static const Container kContainers[] = {{"RVZ\x01", "RVZ"}, {"WIA\x01", "WIA"}, {"CISO", "CISO"}, {"WBFS", "WBFS"}};
  for (const Container& c : kContainers)
    if (std::memcmp(head, c.tag, 4) == 0)
      return Fail(info, DiscStatus::Compressed,
                  std::string("This is a compressed ") + c.name +
                      " image. In Dolphin, right-click the game, choose Convert File..., pick ISO and select the new .iso here.",
                  c.name);
  if (magic == 0x01C00BB1u)  // GCZ: B10BC001 stored little-endian
    return Fail(info, DiscStatus::Compressed,
                "This is a compressed GCZ image. In Dolphin, right-click the game, choose Convert File..., pick ISO and select the new .iso here.",
                "GCZ");
  if (magic == 0xAE0F38A2u)
    return Fail(info, DiscStatus::Compressed, "This is a TGC file, not a full disc. Please choose a plain .iso of the disc.", "TGC");
  if (headBytes < sizeof head) return Fail(info, DiscStatus::NotADisc, "This file is too small to be a GameCube disc.");

  if (BE32(head + 0x18) == 0x5D1C9EA3u)
    return Fail(info, DiscStatus::Wii, "That's a Wii disc. SIMMY needs The Sims 2 for the Nintendo GameCube.");
  if (BE32(head + 0x1C) != 0xC2339F3Du)
    return Fail(info, DiscStatus::NotADisc, "This doesn't look like a GameCube disc image. Choose a .iso or .gcm file.");

  info.gameId.assign(reinterpret_cast<const char*>(head), 6);
  for (char& c : info.gameId)
    if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) c = '?';
  info.discNumber = head[6];
  info.version = head[7];
  unsigned char bi2Region[4] = {};
  if (rd.read(0x458, bi2Region, 4)) info.regionCode = static_cast<int>(BE32(bi2Region));
  const bool japanese = info.gameId[3] == 'J' || info.regionCode == 0;
  info.bootTitle = Latin1ToUtf8(head + 0x20, 0x3E0, japanese);

  unsigned char nkit[4] = {};
  if (rd.read(0x200, nkit, 4) && std::memcmp(nkit, "NKIT", 4) == 0)
    return Fail(info, DiscStatus::NKit,
                "This is an NKit image. Convert it back to a full .iso (NKit's Recover, or Dolphin's Convert File...) and choose that.",
                "NKit");

  // The file system table: the banner, and a quick check that the image is complete.
  const uint64_t fstOffset = BE32(head + 0x424), fstSize = BE32(head + 0x428);
  if (fstSize < 12 || fstOffset + fstSize > rd.size)
    return Fail(info, DiscStatus::Damaged,
                "This disc image is incomplete (trimmed or cut short). Make a full copy of the disc and try again.");
  std::vector<unsigned char> fst(static_cast<size_t>(fstSize));
  if (!rd.read(fstOffset, fst.data(), fst.size()))
    return Fail(info, DiscStatus::Damaged, "SIMMY couldn't read this disc image's file table.");
  const uint32_t count = BE32(&fst[8]);
  if (count == 0 || static_cast<uint64_t>(count) * 12 > fst.size())
    return Fail(info, DiscStatus::Damaged, "This disc image's file table is damaged.");
  const size_t namesAt = static_cast<size_t>(count) * 12;
  auto entryName = [&](uint32_t i) {
    const size_t at = namesAt + (BE32(&fst[i * 12]) & 0xFFFFFFu);
    std::string name;
    for (size_t k = at; k < fst.size() && fst[k]; ++k) name += static_cast<char>(fst[k]);
    return name;
  };
  // Every file must lie inside the image (a cut-short copy fails here; trimmed padding is fine).
  uint64_t dataEnd = 0;
  for (uint32_t i = 1; i < count; ++i)
    if (fst[i * 12] == 0) {
      const uint64_t end = static_cast<uint64_t>(BE32(&fst[i * 12 + 4])) + BE32(&fst[i * 12 + 8]);
      if (end > dataEnd) dataEnd = end;
    }
  if (dataEnd > rd.size)
    return Fail(info, DiscStatus::Damaged,
                "This disc image is incomplete (it ends before the game's files do). Make a full copy of the disc and try again.");
  int bannerEntry = -1;
  for (uint32_t i = 1; i < count;) {
    const bool dir = fst[i * 12] != 0;
    std::string name = entryName(i);
    for (char& c : name) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    if (!dir && name == "opening.bnr") {
      bannerEntry = static_cast<int>(i);
      break;
    }
    // A directory's second word is its parent, its third the index after its last entry.
    const uint32_t next = dir ? BE32(&fst[i * 12 + 8]) : i + 1;
    i = next > i ? next : i + 1;
  }
  if (bannerEntry > 0) {
    const uint64_t at = BE32(&fst[bannerEntry * 12 + 4]), bytes = BE32(&fst[bannerEntry * 12 + 8]);
    std::vector<unsigned char> bnr(0x1960, 0);
    if (bytes >= 0x1820 && rd.read(at, bnr.data(), static_cast<size_t>(bytes < bnr.size() ? bytes : bnr.size())) &&
        (std::memcmp(bnr.data(), "BNR1", 4) == 0 || std::memcmp(bnr.data(), "BNR2", 4) == 0)) {
      info.hasBanner = true;
      decode_rgb5a3_banner(&bnr[0x20], info.bannerRgba);
      if (bytes >= 0x1960) {
        const unsigned char* n = &bnr[0x1820];  // the first (English) set
        info.shortName = Latin1ToUtf8(n, 0x20, japanese);
        info.shortMaker = Latin1ToUtf8(n + 0x20, 0x20, japanese);
        info.longName = Latin1ToUtf8(n + 0x40, 0x40, japanese);
        info.longMaker = Latin1ToUtf8(n + 0x80, 0x40, japanese);
        info.description = Latin1ToUtf8(n + 0xC0, 0x80, japanese);
      }
    }
  }

  if (info.gameId.compare(0, 3, "G4Z") != 0) {
    const std::string title = info.displayTitle();
    return Fail(info, DiscStatus::WrongGame,
                "That's " + (title.empty() ? std::string("another game") : "\"" + title + "\"") + " (" + info.gameId +
                    "). SIMMY needs The Sims 2 for the GameCube.");
  }
  info.status = DiscStatus::Ok;
  return info;
}

std::string DiscInfo::displayTitle() const {
  if (!longName.empty()) return longName;
  if (!shortName.empty()) return shortName;
  return bootTitle;
}

std::string DiscInfo::displayMaker() const { return !longMaker.empty() ? longMaker : shortMaker; }

std::string DiscInfo::regionName() const {
  if (gameId.size() >= 4) switch (gameId[3]) {
    case 'E': return "USA";
    case 'P': return "Europe";
    case 'J': return "Japan";
    case 'D': return "Germany";
    case 'F': return "France";
    case 'S': return "Spain";
    case 'I': return "Italy";
    case 'H': return "Netherlands";
    case 'U': return "Australia";
    case 'K': return "Korea";
    case 'X': case 'Y': case 'Z': return "Europe";
    default: break;
    }
  switch (regionCode) {
  case 0: return "Japan";
  case 1: return "Americas";
  case 2: return "Europe";
  case 4: return "Korea";
  default: return "Unknown region";
  }
}

std::string DiscInfo::videoName() const {
  switch (regionCode) {
  case 0: return "NTSC-J";
  case 1: return "NTSC-U";
  case 2: return "PAL";
  case 4: return "NTSC-K";
  default: return "";
  }
}

}  // namespace launcher
