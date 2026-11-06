// SimmyLauncher: simmy.ini editing (see launcher_ini.hpp).
#include "launcher_ini.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#include <cctype>

namespace launcher {

namespace {
std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}
// The value part of an active line: up to a ; or # comment, trimmed.
std::string ValueOf(const std::string& line) {
  const size_t eq = line.find('=');
  if (eq == std::string::npos) return "";
  std::string v = line.substr(eq + 1);
  const size_t comment = v.find_first_of(";#");
  if (comment != std::string::npos) v.erase(comment);
  return Trim(v);
}
}  // namespace

std::string IniFile::normalizeKey(const std::string& key) {
  // Same as the game's NormalizeKey (src/platform/pc_settings.cpp).
  std::string out;
  for (char c : Lower(Trim(key)))
    if (c != '-' && c != '_') out += c;
  if (out == "render" || out == "renderresolution" || out == "internalresolution" || out == "res") return "resolution";
  if (out == "framecap" || out == "fps" || out == "maxfps" || out == "fpslimit") return "fpscap";
  if (out == "aniso" || out == "anisotropic" || out == "anisotropicfiltering") return "anisotropy";
  if (out == "windowsize") return "window";
  if (out == "antialiasing" || out == "aa") return "msaa";
  if (out == "renderer" || out == "api") return "backend";
  if (out == "coop" || out == "coopdisplay" || out == "splitscreen") return "split";
  if (out == "window2size" || out == "player2window") return "window2";
  if (out == "window2position") return "window2pos";
  if (out == "window2monitor" || out == "display2" || out == "monitor2") return "window2display";
  if (out == "window2fullscreen") return "fullscreen2";
  if (out == "player1input" || out == "p1controls" || out == "player1controls" || out == "input") return "p1input";
  if (out == "edgepan" || out == "edgepanning" || out == "mouseedgepanning") return "mouseedgepan";
  return out;
}

bool IniFile::load(const std::string& path) {
  path_ = path;
  lines_.clear();
  crlf_ = false;
  size_t size = 0;
  void* data = SDL_LoadFile(path.c_str(), &size);
  existed_ = data != nullptr;
  if (!data) return false;
  std::string text(static_cast<const char*>(data), size);
  SDL_free(data);
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF)
    text.erase(0, 3);  // a UTF-8 BOM (Notepad)
  crlf_ = text.find("\r\n") != std::string::npos;
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines_.push_back(line);
    start = end + 1;
  }
  return true;
}

std::vector<IniFile::Parsed> IniFile::parse() const {
  std::vector<Parsed> out(lines_.size());
  std::string section;
  for (size_t i = 0; i < lines_.size(); ++i) {
    const std::string& line = lines_[i];
    Parsed& p = out[i];
    const size_t lead = line.find_first_not_of(" \t");
    if (lead == std::string::npos) {
      p.section = section;
      continue;
    }
    if (line[lead] == '[') {
      const size_t close = line.find(']', lead);
      section = Lower(Trim(line.substr(lead + 1, close == std::string::npos ? std::string::npos : close - lead - 1)));
      p.header = true;
      p.section = section;
      continue;
    }
    p.section = section;
    const bool isComment = line[lead] == ';' || line[lead] == '#';
    const size_t keyStart = isComment ? line.find_first_not_of(" \t;#", lead) : lead;
    if (keyStart == std::string::npos) continue;
    const size_t eq = line.find('=', keyStart);
    if (eq == std::string::npos) continue;
    const std::string rawKey = line.substr(keyStart, eq - keyStart);
    if (rawKey.find_first_of(";#") != std::string::npos) continue;
    p.key = normalizeKey(rawKey);
    if (isComment) {
      const std::string rest = Trim(line.substr(eq + 1));
      p.commented = rest.find_first_of(" \t") == std::string::npos;  // "; key = value", not prose
    } else {
      p.active = true;
    }
  }
  return out;
}

std::string IniFile::get(const std::string& key) const {
  const std::string k = normalizeKey(key);
  const std::vector<Parsed> p = parse();
  std::string value;
  for (size_t i = 0; i < p.size(); ++i)
    if (p[i].active && p[i].key == k && p[i].section != "launcher" && p[i].section != "keys") value = ValueOf(lines_[i]);
  return value;
}

std::string IniFile::getLauncher(const std::string& key) const {
  const std::string k = normalizeKey(key);
  const std::vector<Parsed> p = parse();
  std::string value;
  for (size_t i = 0; i < p.size(); ++i)
    if (p[i].active && p[i].key == k && p[i].section == "launcher") value = ValueOf(lines_[i]);
  return value;
}

void IniFile::replaceValue(size_t i, const std::string& value) {
  // Keep the key's spelling, the spacing and a trailing comment (as the game does).
  std::string& line = lines_[i];
  const size_t eq = line.find('=');
  const size_t valueStart = line.find_first_not_of(" \t", eq + 1);
  const size_t comment = line.find_first_of(";#", eq + 1);
  std::string tail;
  if (comment != std::string::npos) {
    size_t cut = comment;
    while (cut > eq + 1 && (line[cut - 1] == ' ' || line[cut - 1] == '\t')) --cut;
    tail = line.substr(cut);
  }
  const std::string pad = valueStart == std::string::npos || (comment != std::string::npos && valueStart >= comment)
                              ? std::string(" ")
                              : line.substr(eq + 1, valueStart - eq - 1);
  line = line.substr(0, eq + 1) + pad + value + tail;
}

int IniFile::firstHeader(const std::vector<Parsed>& p, bool skipLauncher) const {
  for (size_t i = 0; i < p.size(); ++i)
    if (p[i].header && !(skipLauncher && p[i].section == "launcher")) return static_cast<int>(i);
  return -1;
}

void IniFile::setGraphics(const std::string& name, const std::string& value, bool isDefault) {
  const std::string k = normalizeKey(name);
  std::vector<Parsed> p = parse();
  bool found = false;
  int commented = -1;
  for (size_t i = 0; i < p.size(); ++i) {
    if (p[i].key != k || p[i].section == "launcher" || p[i].section == "keys") continue;
    if (p[i].active) replaceValue(i, value), found = true;
    else if (p[i].commented && commented < 0) commented = static_cast<int>(i);
  }
  if (found || isDefault) return;
  if (commented >= 0) {
    lines_[static_cast<size_t>(commented)] = name + " = " + value;
    return;
  }
  // Appended at the end, which must not be inside [launcher] (the game skips that section) or [keys]
  // (player 1's key bindings).
  if (lines_.empty()) {
    lines_.push_back("; SIMMY settings (written by SimmyLauncher). See simmy.ini.example.");
    lines_.push_back("");
    lines_.push_back("[graphics]");
  } else if (firstHeader(p, false) < 0 || (!p.empty() && (p.back().section == "launcher" || p.back().section == "keys"))) {
    if (!Trim(lines_.back()).empty()) lines_.push_back("");
    lines_.push_back("[graphics]");
  }
  lines_.push_back(name + " = " + value);
}

void IniFile::setTopLevel(const std::string& name, const std::string& value, const std::string& comment) {
  const std::string k = normalizeKey(name);
  std::vector<Parsed> p = parse();
  bool found = false;
  int commented = -1;
  for (size_t i = 0; i < p.size(); ++i) {
    if (p[i].key != k || p[i].section == "launcher") continue;
    if (p[i].active) replaceValue(i, value), found = true;
    else if (p[i].commented && commented < 0) commented = static_cast<int>(i);
  }
  if (found) return;
  if (commented >= 0) {
    lines_[static_cast<size_t>(commented)] = name + " = " + value;
    return;
  }
  const int header = firstHeader(p, false);
  std::vector<std::string> add;
  if (!comment.empty()) add.push_back("; " + comment);
  add.push_back(name + " = " + value);
  if (header < 0) {
    if (!lines_.empty() && !Trim(lines_.back()).empty()) lines_.push_back("");
    lines_.insert(lines_.end(), add.begin(), add.end());
  } else {
    add.push_back("");
    lines_.insert(lines_.begin() + header, add.begin(), add.end());
  }
}

void IniFile::setLauncher(const std::string& name, const std::string& value) {
  const std::string k = normalizeKey(name);
  std::vector<Parsed> p = parse();
  int last = -1, header = -1;
  bool found = false;
  for (size_t i = 0; i < p.size(); ++i) {
    if (p[i].section != "launcher") continue;
    if (p[i].header) header = static_cast<int>(i);
    if (p[i].active && p[i].key == k) replaceValue(i, value), found = true;
    if (!Trim(lines_[i]).empty()) last = static_cast<int>(i);
  }
  if (found) return;
  if (header >= 0) {
    lines_.insert(lines_.begin() + (last + 1), name + " = " + value);
    return;
  }
  // A new [launcher] section, before the game's first section (keys the game appends then stay out
  // of it). Without any section a [graphics] header follows it for the same reason.
  const int before = firstHeader(p, true);
  std::vector<std::string> add = {"[launcher]", "; SimmyLauncher's own settings (the game skips this section).",
                                  name + " = " + value, ""};
  if (before < 0) {
    if (!lines_.empty() && !Trim(lines_.back()).empty()) lines_.push_back("");
    lines_.insert(lines_.end(), add.begin(), add.end());
    lines_.push_back("[graphics]");
  } else {
    lines_.insert(lines_.begin() + before, add.begin(), add.end());
  }
}

bool IniFile::save(std::string& message) {
  std::string text;
#ifdef _WIN32
  const char* eol = existed_ && !crlf_ ? "\n" : "\r\n";
#else
  const char* eol = crlf_ ? "\r\n" : "\n";
#endif
  for (const std::string& line : lines_) text += line + eol;
  const std::string tmp = path_ + ".tmp";
  SDL_IOStream* io = SDL_IOFromFile(tmp.c_str(), "wb");
  bool ok = io != nullptr;
  if (io) {
    ok = SDL_WriteIO(io, text.data(), text.size()) == text.size();
    ok = SDL_CloseIO(io) && ok;
  }
  if (ok) ok = SDL_RenamePath(tmp.c_str(), path_.c_str());
  if (!ok) {
    message = std::string("Couldn't save ") + path_ + ": " + SDL_GetError();
    SDL_RemovePath(tmp.c_str());
    return false;
  }
  existed_ = true;
  message = "Saved to simmy.ini";
  return true;
}

}  // namespace launcher
