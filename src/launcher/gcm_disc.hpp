// SimmyLauncher: a small reader for GameCube disc images (GCM / plain .iso), just enough to check the
// disc the player picked and show its banner. The game has its own disc reader; this one is
// independent of it.
//
// Layout used (all values big-endian):
//   boot.bin  0x000 game ID (6 chars), 0x006 disc number, 0x007 version, 0x018 Wii magic,
//             0x01C GameCube magic C2339F3D, 0x020 game title, 0x424 FST offset, 0x428 FST size
//   bi2.bin   0x440 (+0x18 = 0x458 region: 0 NTSC-J, 1 NTSC-U, 2 PAL, 4 NTSC-K)
//   FST       12-byte entries (flags/name offset, data offset or parent, size or next index), then the
//             name table; the root's third word is the number of entries.
//   opening.bnr  "BNR1"/"BNR2", 96x32 RGB5A3 image at 0x20 (4x4 tiles), names from 0x1820
//             (0x140 bytes a language: short name, short maker, long name, long maker, description).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace launcher {

enum class DiscStatus {
  Ok,           // a GameCube disc of The Sims 2
  WrongGame,    // a GameCube disc, but another game
  Missing,      // the file cannot be opened
  NotADisc,     // unknown data
  Compressed,   // RVZ / WIA / CISO / GCZ / WBFS / TGC: convert it with Dolphin first
  NKit,         // NKit image: convert it back with NKit / Dolphin
  Wii,          // a Wii disc
  Damaged,      // looks like a GameCube disc but its file table is out of range (trimmed / cut short)
};

struct DiscInfo {
  DiscStatus status = DiscStatus::Missing;
  std::string path;
  std::string message;      // a short user-facing explanation when status != Ok
  std::string format;       // "RVZ", "CISO", ... for Compressed / NKit
  std::string gameId;       // "G4ZE69"
  std::string bootTitle;    // boot.bin's title (UTF-8)
  int discNumber = 0, version = 0;
  int regionCode = -1;      // bi2.bin's region word, -1 if unknown
  uint64_t fileSize = 0;
  // From opening.bnr (empty if the disc has none).
  bool hasBanner = false;
  std::vector<uint32_t> bannerRgba;  // 96*32 pixels, RGBA8 in memory byte order R,G,B,A
  std::string shortName, shortMaker, longName, longMaker, description;  // UTF-8

  bool ok() const { return status == DiscStatus::Ok; }
  // The name to show: the banner's long name, else its short name, else boot.bin's title.
  std::string displayTitle() const;
  std::string displayMaker() const;
  // "USA", "Europe", "Japan", ... from the game ID's region letter (falls back to bi2's region).
  std::string regionName() const;
  // "NTSC-U", "PAL", ... from bi2.bin.
  std::string videoName() const;
};

constexpr int kBannerWidth = 96, kBannerHeight = 32;

// Opens and checks a disc image (UTF-8 path).
DiscInfo probe_disc(const std::string& path);

// Decodes 96x32 RGB5A3 (4x4 tiles, big-endian) into RGBA8 pixels (byte order R,G,B,A).
void decode_rgb5a3_banner(const unsigned char* src, std::vector<uint32_t>& out);

}  // namespace launcher
