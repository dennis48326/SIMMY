// SimmyLauncher: reads and edits simmy.ini (the game's settings file next to simmy.exe) line by line,
// keeping comments, layout and unknown lines.
//
// The launcher writes three kinds of keys:
//   iso = <path>      the disc image, before the first [section] (the game reads it, also --iso)
//   [launcher]        the launcher's own settings (skip_intro, quick_coop, coop_display, sounds); the
//                     section goes before [graphics], so keys the game appends land in [graphics]
//   [graphics] keys   window, fullscreen, ... written like the game's own F1 "Save" does
//                     (pc_settings.cpp SaveNames): every active line of the key gets the new value; a
//                     key without one replaces its commented-out example line, else it is appended. A
//                     key set back to its default is only rewritten where the file already sets it.
#pragma once
#include <string>
#include <vector>

namespace launcher {

class IniFile {
 public:
  // Reads `path`; a missing file is fine (saving creates it). Returns false if it does not exist.
  bool load(const std::string& path);
  const std::string& path() const { return path_; }
  bool existed() const { return existed_; }

  // The value of a key outside [launcher] (the last active line wins, as in the game), or "" if unset.
  std::string get(const std::string& key) const;
  // A key in [launcher].
  std::string getLauncher(const std::string& key) const;

  void setGraphics(const std::string& name, const std::string& value, bool isDefault);
  void setTopLevel(const std::string& name, const std::string& value, const std::string& comment);
  void setLauncher(const std::string& name, const std::string& value);

  bool save(std::string& message);

  // "fps-cap", "FPS_CAP" and "fpscap" are the same key (the game's aliases included).
  static std::string normalizeKey(const std::string& key);

 private:
  struct Parsed {
    bool active = false;     // an active key = value line
    bool commented = false;  // "; key = value" with a bare value (an example line)
    std::string key;         // normalized
    std::string section;     // lower-case section name ("" before the first header)
    bool header = false;
  };
  std::vector<Parsed> parse() const;
  void replaceValue(size_t line, const std::string& value);
  int firstHeader(const std::vector<Parsed>& p, bool skipLauncher) const;

  std::string path_;
  std::vector<std::string> lines_;
  bool existed_ = false;
  bool crlf_ = false;
};

}  // namespace launcher
