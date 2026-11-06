// PC: graphics settings (see pc_settings.hpp and simmy.ini.example).
#include "pc_settings.hpp"

#include "simmy/platform.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <aurora/gfx.hpp>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/vi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// Anisotropic filtering at run time: aurora builds each texture's sampler descriptor from this value
// whenever the texture is bound (gfx/texture.cpp wgpu_aniso; samplers are cached per descriptor), so
// changing it applies to the next frame. Same definition as aurora's lib/webgpu/gpu.hpp (internal).
namespace aurora::webgpu {
struct GraphicsConfig {
  wgpu::TextureFormat depthFormat;
  uint32_t msaaSamples;
  uint16_t textureAnisotropy;
  bool normalBuffer = false;
};
extern GraphicsConfig g_graphicsConfig;
}  // namespace aurora::webgpu

namespace simmy {

namespace {
std::vector<std::pair<std::string, std::string>> g_cliValues;  // recorded switches, in order
std::string g_configPath;                                       // --config <file>
std::string g_exeDir;                                           // where simmy.ini is looked for
bool g_fullscreenNow = false;                                    // current state (Alt+Enter)
bool g_offscreen = false;                                        // aurora's headless surface (tests)

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
// "fps-cap", "FPS_CAP" and "fpscap" are the same key.
std::string NormalizeKey(const std::string& key) {
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
  if (out == "disc" || out == "discimage" || out == "isopath" || out == "gcm") return "iso";
  if (out == "player1input" || out == "p1controls" || out == "player1controls" || out == "input") return "p1input";
  if (out == "edgepan" || out == "edgepanning" || out == "mouseedgepanning") return "mouseedgepan";
  return out;
}

const char* const kKeys[] = {"window", "fullscreen", "resolution", "aspect", "hud", "msaa",
                             "anisotropy", "vsync", "fpscap", "backend", "resampler", "split",
                             "window2", "window2pos", "window2display", "fullscreen2", "iso",
                             "p1input", "mouseedgepan", "mouserotatespeed", "mousezoomspeed", "mouseinvertx",
                             "mouseinverty", "mouseinvertwheel"};
bool KnownKey(const std::string& k) {
  for (const char* known : kKeys)
    if (k == known) return true;
  return false;
}

bool ParseBool(const std::string& v, bool& out) {
  const std::string s = Lower(Trim(v));
  if (s == "1" || s == "on" || s == "true" || s == "yes") return out = true, true;
  if (s == "0" || s == "off" || s == "false" || s == "no") return out = false, true;
  return false;
}
bool ParseSize(const std::string& v, unsigned& w, unsigned& h) {
  const std::string s = Lower(Trim(v));
  const size_t x = s.find('x');
  if (x == std::string::npos) return false;
  const long a = std::strtol(s.c_str(), nullptr, 10), b = std::strtol(s.c_str() + x + 1, nullptr, 10);
  if (a < 160 || b < 120 || a > 16384 || b > 16384) return false;
  w = static_cast<unsigned>(a), h = static_cast<unsigned>(b);
  return true;
}

void Warn(const std::string& key, const std::string& value, const char* expected) {
  std::fprintf(stderr, "[gfx] ignoring %s=%s (expected %s)\n", key.c_str(), value.c_str(), expected);
}

// One key=value onto the settings; `key` is normalized.
void ApplyValue(GraphicsSettings& g, const std::string& key, const std::string& rawValue) {
  if (key == "iso") {  // a path: case and inner spaces kept
    std::string path = Trim(rawValue);
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') path = path.substr(1, path.size() - 2);
    g.isoPath = path;
    return;
  }
  const std::string v = Lower(Trim(rawValue));
  if (v.empty()) return;
  if (key == "window") {
    if (v == "default" || v == "auto") g.windowSet = false, g.windowWidth = g.windowHeight = 0;
    else if (ParseSize(v, g.windowWidth, g.windowHeight)) g.windowSet = true;
    else Warn(key, v, "WIDTHxHEIGHT, e.g. 1920x1080");
  } else if (key == "fullscreen") {
    bool b = false;
    if (v == "borderless" || v == "desktop") g.fullscreen = FullscreenMode::Borderless;
    else if (v == "exclusive") g.fullscreen = FullscreenMode::Exclusive;
    else if (v == "windowed") g.fullscreen = FullscreenMode::Off;
    else if (ParseBool(v, b)) g.fullscreen = b ? FullscreenMode::Borderless : FullscreenMode::Off;
    else Warn(key, v, "off, borderless or exclusive");
  } else if (key == "resolution") {
    g.renderHeight = 0, g.renderScale = 0.0f;
    if (v == "auto" || v == "window" || v == "native") return;
    if (v == "4k") return void(g.renderHeight = 2160);
    if (v.size() > 1 && v.back() == 'p') {
      const long h = std::strtol(v.c_str(), nullptr, 10);
      if (h >= 240 && h <= 8640) return void(g.renderHeight = static_cast<unsigned>(h));
    } else if (v.size() > 1 && v.back() == 'x') {
      const double s = std::strtod(v.c_str(), nullptr);
      if (s >= 0.5 && s <= 16.0) return void(g.renderScale = static_cast<float>(s));
    } else {
      unsigned w = 0, h = 0;
      if (ParseSize(v, w, h)) return void(g.renderHeight = h);  // the width follows the aspect
      char* end = nullptr;
      const double s = std::strtod(v.c_str(), &end);
      if (end && *end == '\0' && s >= 0.5 && s <= 16.0) return void(g.renderScale = static_cast<float>(s));
    }
    Warn(key, v, "auto, 480p, 720p, 1080p, 1440p, 2160p or a scale such as 2x");
  } else if (key == "aspect") {
    if (v == "4:3" || v == "gc" || v == "4x3" || v == "original") g.wide = false, g.defaultAspect = 4.0f / 3.0f;
    else if (v == "16:9" || v == "wide" || v == "16x9" || v == "widescreen") g.wide = true, g.defaultAspect = 16.0f / 9.0f;
    else if (v == "16:10" || v == "16x10") g.wide = true, g.defaultAspect = 1.6f;
    else if (v == "21:9" || v == "21x9") g.wide = true, g.defaultAspect = 21.0f / 9.0f;
    else Warn(key, v, "4:3 or 16:9 (also 16:10, 21:9)");
  } else if (key == "hud") {
    if (v == "centered" || v == "centred" || v == "center" || v == "centre") g.hud = HudMode::Centered;
    else if (v == "edges" || v == "edge" || v == "anchored") g.hud = HudMode::Edges;
    else Warn(key, v, "centered or edges");
  } else if (key == "msaa") {
    const long n = std::strtol(v.c_str(), nullptr, 10);
    bool b = false;
    if (n == 1 || n == 2 || n == 4 || n == 8) g.msaa = static_cast<unsigned>(n);
    else if (ParseBool(v, b) && !b) g.msaa = 1;
    else Warn(key, v, "1, 2, 4 or 8");
  } else if (key == "anisotropy") {
    const long n = std::strtol(v.c_str(), nullptr, 10);
    bool b = true;
    if (n >= 1 && n <= 16) g.anisotropy = static_cast<unsigned>(n);
    else if (ParseBool(v, b) && !b) g.anisotropy = 1;
    else Warn(key, v, "1 to 16");
  } else if (key == "vsync") {
    if (!ParseBool(v, g.vsync)) Warn(key, v, "on or off");
  } else if (key == "fpscap") {
    const long n = std::strtol(v.c_str(), nullptr, 10);
    if (v == "off" || v == "0" || v == "unlimited" || v == "uncapped" || n > 60) {
      // The engine measures each frame in 60 Hz retraces (EEngine::RetraceUpdate rounds up to at least
      // one), so frames faster than 60 Hz would run the game fast. 60 is the ceiling.
      g.fpsCap = 60;
      std::fprintf(stderr, "[gfx] fps_cap=%s: the game runs on 60 Hz retraces; capping at 60\n", v.c_str());
    } else if (n == 60 || n == 30 || n == 20 || n == 15) {
      g.fpsCap = static_cast<unsigned>(n);
    } else if (n > 0) {
      // A whole number of retraces per frame keeps the game's timing exact: round to 60/k.
      const long k = std::max(1L, std::min(4L, std::lround(60.0 / static_cast<double>(n))));
      g.fpsCap = static_cast<unsigned>(60 / k);
      std::fprintf(stderr, "[gfx] fps_cap=%ld: using %u (whole 60 Hz retraces per frame)\n", n, g.fpsCap);
    } else {
      Warn(key, v, "60, 30, 20 or 15");
    }
  } else if (key == "backend") {
    if (v == "auto" || v == "default") g.backend = BACKEND_AUTO;
    else if (v == "d3d12" || v == "dx12") g.backend = BACKEND_D3D12;
    else if (v == "d3d11" || v == "dx11") g.backend = BACKEND_D3D11;
    else if (v == "vulkan" || v == "vk") g.backend = BACKEND_VULKAN;
    else if (v == "metal") g.backend = BACKEND_METAL;
    else if (v == "opengl" || v == "gl") g.backend = BACKEND_OPENGL;
    else if (v == "opengles" || v == "gles") g.backend = BACKEND_OPENGLES;
    else if (v == "null") g.backend = BACKEND_NULL;
    else Warn(key, v, "auto, d3d12, d3d11, vulkan, metal, opengl or opengles");
  } else if (key == "resampler") {
    if (v == "bilinear" || v == "linear") g.areaResampler = false;
    else if (v == "area" || v == "box") g.areaResampler = true;
    else Warn(key, v, "bilinear or area");
  } else if (key == "split") {
    if (v == "gc" || v == "original" || v == "4:3") g.split = 0;
    else if (v == "4x3" || v == "coop4x3") g.split = 1;
    else if (v == "wide" || v == "widescreen") g.split = 2;
    else if (v == "windows" || v == "window" || v == "2windows" || v == "two-windows" || v == "dual") g.split = 3;
    else Warn(key, v, "gc, 4x3, wide or windows");
  } else if (key == "window2") {
    if (v == "default" || v == "auto") g.window2Set = false, g.window2Width = g.window2Height = 0;
    else if (ParseSize(v, g.window2Width, g.window2Height)) g.window2Set = true;
    else Warn(key, v, "WIDTHxHEIGHT, e.g. 1920x1080");
  } else if (key == "window2pos") {
    const size_t comma = v.find_first_of(",x ");
    if (v == "default" || v == "auto") {
      g.window2PosSet = false;
    } else if (comma != std::string::npos) {
      char* end = nullptr;
      const long x = std::strtol(v.c_str(), &end, 10), y = std::strtol(v.c_str() + comma + 1, nullptr, 10);
      if (end && end != v.c_str() && x > -100000 && x < 100000 && y > -100000 && y < 100000)
        g.window2X = static_cast<int>(x), g.window2Y = static_cast<int>(y), g.window2PosSet = true;
      else Warn(key, v, "X,Y (desktop pixels) or auto");
    } else {
      Warn(key, v, "X,Y (desktop pixels) or auto");
    }
  } else if (key == "window2display") {
    char* end = nullptr;
    const long n = std::strtol(v.c_str(), &end, 10);
    if (v == "default" || v == "auto") g.window2Display = -1;
    else if (end && *end == '\0' && n >= 0 && n < 64) g.window2Display = static_cast<int>(n);
    else Warn(key, v, "a display number (0 = the first) or auto");
  } else if (key == "fullscreen2") {
    bool b = false;
    if (v == "borderless" || v == "desktop" || v == "exclusive") g.fullscreen2 = true;
    else if (v == "windowed") g.fullscreen2 = false;
    else if (ParseBool(v, b)) g.fullscreen2 = b;
    else Warn(key, v, "off or borderless");
  } else if (key == "p1input") {
    if (v == "auto" || v == "both" || v == "default") g.p1Input = 0;
    else if (v == "controller" || v == "pad" || v == "gamepad") g.p1Input = 1;
    else if (v == "keyboard_mouse" || v == "keyboardmouse" || v == "keyboard" || v == "kbm" || v == "mouse" ||
             v == "keyboard+mouse")
      g.p1Input = 2;
    else Warn(key, v, "auto, controller or keyboard_mouse");
  } else if (key == "mouseedgepan") {
    bool b = false;
    if (v == "fullscreen" || v == "auto") g.mouseEdgePan = 2;
    else if (ParseBool(v, b)) g.mouseEdgePan = b ? 1 : 0;
    else Warn(key, v, "off, on or fullscreen");
  } else if (key == "mouserotatespeed" || key == "mousezoomspeed") {
    char* end = nullptr;
    const double f = std::strtod(v.c_str(), &end);
    if (end && *end == '\0' && f >= 0.1 && f <= 10.0) (key == "mouserotatespeed" ? g.mouseRotateSpeed : g.mouseZoomSpeed) = static_cast<float>(f);
    else Warn(key, v, "a number from 0.1 to 10 (1 = default)");
  } else if (key == "mouseinvertx" || key == "mouseinverty" || key == "mouseinvertwheel") {
    bool& b = key == "mouseinvertx" ? g.mouseInvertX : key == "mouseinverty" ? g.mouseInvertY : g.mouseInvertWheel;
    if (!ParseBool(v, b)) Warn(key, v, "on or off");
  }
}

// Reads a whole text file through SDL (UTF-8 paths, also on Windows) into lines without their line
// ends. Returns false if it cannot be read.
bool ReadLines(const std::string& path, std::vector<std::string>& lines) {
  size_t size = 0;
  void* data = SDL_LoadFile(path.c_str(), &size);
  if (!data) return false;
  const std::string text(static_cast<const char*>(data), size);
  SDL_free(data);
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
    start = end + 1;
  }
  return true;
}

void LoadIni(GraphicsSettings& g, const std::string& path, bool required) {
  std::vector<std::string> lines;
  if (!ReadLines(path, lines)) {
    if (required) std::fprintf(stderr, "[gfx] cannot read %s\n", path.c_str());
    return;
  }
  g.sourceFile = path;
  int lineNo = 0;
  bool launcherSection = false, keysSection = false;
  for (std::string line : lines) {
    ++lineNo;
    // iso= holds a path, which may contain ';' or '#': there the rest of the line is the value.
    if (const size_t lead = line.find_first_not_of(" \t"); lead != std::string::npos && line[lead] != ';' &&
                                                           line[lead] != '#' && line.find('=') != std::string::npos &&
                                                           NormalizeKey(line.substr(0, line.find('='))) == "iso") {
      ApplyValue(g, "iso", line.substr(line.find('=') + 1));
      continue;
    }
    const size_t comment = line.find_first_of(";#");
    if (comment != std::string::npos) line.erase(comment);
    line = Trim(line);
    // [graphics] sections are allowed and ignored; [launcher] holds SimmyLauncher's own settings;
    // [keys] player 1's key bindings (pc_kbm.cpp).
    if (!line.empty() && line[0] == '[') {
      launcherSection = Lower(line) == "[launcher]";
      keysSection = Lower(line) == "[keys]";
    }
    if (line.empty() || line[0] == '[' || launcherSection) continue;
    const size_t eq = line.find('=');
    if (keysSection) {
      GraphicsSettings::KeyLine k;
      k.action = Trim(eq == std::string::npos ? line : line.substr(0, eq));
      k.keys = eq == std::string::npos ? std::string() : Trim(line.substr(eq + 1));
      k.where = path + ":" + std::to_string(lineNo);
      if (eq == std::string::npos) std::fprintf(stderr, "[keys] %s: \"%s\" is not \"action = key, key\"\n", k.where.c_str(), line.c_str());
      else g.keyLines.push_back(k);
      continue;
    }
    const std::string key = NormalizeKey(eq == std::string::npos ? line : line.substr(0, eq));
    if (eq == std::string::npos || !KnownKey(key)) {
      std::fprintf(stderr, "[gfx] %s:%d: unknown setting \"%s\"\n", path.c_str(), lineNo, line.c_str());
      continue;
    }
    ApplyValue(g, key, line.substr(eq + 1));
  }
}

std::string ExeDir(const char* argv0) {
  if (const char* base = SDL_GetBasePath()) return base;  // owned by SDL, ends with a separator
  std::string p = argv0 ? argv0 : "";
  const size_t slash = p.find_last_of("/\\");
  return slash == std::string::npos ? std::string() : p.substr(0, slash + 1);
}

const char* BackendName(int b) {
  switch (b) {
  case BACKEND_D3D11: return "d3d11";
  case BACKEND_D3D12: return "d3d12";
  case BACKEND_METAL: return "metal";
  case BACKEND_VULKAN: return "vulkan";
  case BACKEND_OPENGL: return "opengl";
  case BACKEND_OPENGLES: return "opengles";
  case BACKEND_NULL: return "null";
  default: return "auto";
  }
}

// The window size the settings ask for; 0x0 = aurora's default (1280x960).
void ConfiguredSize(const GraphicsSettings& g, unsigned& w, unsigned& h) {
  w = h = 0;
  if (g.windowSet) {
    w = g.windowWidth, h = g.windowHeight;
  } else if (split_mode() != SplitMode::Gc) {
    split_default_frame_size(w, h);  // the split-screen modes' wider window (pc_splitscreen.cpp)
  } else if (g.wide) {
    w = 1280, h = static_cast<unsigned>(std::lround(1280.0 / g.defaultAspect));
  }
}

// The render scale aurora is given (VISetFrameBufferScale): EFB = 640x480 * scale, widened to the
// window's aspect in widescreen. 0 = the window's own pixel size.
float RenderScale(const GraphicsSettings& g) {
  if (g.renderScale > 0.0f) return g.renderScale;
  if (g.renderHeight) return static_cast<float>(g.renderHeight) / 480.0f;
  return 0.0f;
}

void SetFullscreen(bool on) {
  SDL_Window* win = aurora_get_window();
  if (!win) return;
  const GraphicsSettings& g = graphics_settings();
  if (on && g.fullscreen == FullscreenMode::Exclusive) {
    // Exclusive: a real display mode (the window= size if given, else the desktop's).
    const SDL_DisplayID display = SDL_GetDisplayForWindow(win);
    const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display);
    int w = desktop ? desktop->w : 0, h = desktop ? desktop->h : 0;
    if (g.windowSet) w = static_cast<int>(g.windowWidth), h = static_cast<int>(g.windowHeight);
    SDL_DisplayMode mode;
    if (w && h && SDL_GetClosestFullscreenDisplayMode(display, w, h, 0.0f, true, &mode)) {
      if (!SDL_SetWindowFullscreenMode(win, &mode))
        std::fprintf(stderr, "[gfx] exclusive mode %dx%d refused: %s\n", mode.w, mode.h, SDL_GetError());
    } else {
      std::fprintf(stderr, "[gfx] no exclusive display mode near %dx%d; using borderless\n", w, h);
      SDL_SetWindowFullscreenMode(win, nullptr);
    }
  } else if (on) {
    SDL_SetWindowFullscreenMode(win, nullptr);  // borderless (desktop) fullscreen
  }
  if (!SDL_SetWindowFullscreen(win, on)) std::fprintf(stderr, "[gfx] fullscreen: %s\n", SDL_GetError());
  g_fullscreenNow = on;
}
}  // namespace

GraphicsSettings& graphics_settings() {
  static GraphicsSettings settings;
  return settings;
}

bool settings_parse_arg(int argc, char** argv, int& i) {
  const char* a = argv[i];
  if (std::strncmp(a, "--", 2) != 0) return false;
  std::string body = a + 2, value;
  const size_t eq = body.find('=');
  const bool inlineValue = eq != std::string::npos;
  if (inlineValue) value = body.substr(eq + 1), body.erase(eq);
  const std::string key = NormalizeKey(body);
  if (key == "config") {
    if (inlineValue) g_configPath = value;
    else if (i + 1 < argc) g_configPath = argv[++i];
    return true;
  }
  // Flags without a value.
  if (!inlineValue) {
    if (key == "fullscreen" && (i + 1 >= argc || std::strncmp(argv[i + 1], "--", 2) == 0)) {
      g_cliValues.emplace_back("fullscreen", "borderless");
      return true;
    }
    if (key == "windowed") return g_cliValues.emplace_back("fullscreen", "off"), true;
    if (key == "widescreen") return g_cliValues.emplace_back("aspect", "16:9"), true;
    if (key == "vsync" && (i + 1 >= argc || std::strncmp(argv[i + 1], "--", 2) == 0))
      return g_cliValues.emplace_back("vsync", "on"), true;
    if (key == "novsync") return g_cliValues.emplace_back("vsync", "off"), true;
  }
  if (!KnownKey(key)) return false;
  if (!inlineValue) {
    if (i + 1 >= argc) return false;
    value = argv[++i];
  }
  g_cliValues.emplace_back(key, value);
  return true;
}

void settings_load(const char* argv0) {
  GraphicsSettings& g = graphics_settings();
  if (!g_configPath.empty()) LoadIni(g, g_configPath, true);
  else if (const char* env = std::getenv("SIMMY_INI")) LoadIni(g, env, true);
  else LoadIni(g, (g_exeDir = ExeDir(argv0)) + "simmy.ini", false);
  // Environment (tests): SIMMY_GFX="aspect=16:9;msaa=4" and SIMMY_GFX_<KEY>=value.
  if (const char* env = std::getenv("SIMMY_GFX")) {
    std::string all = env;
    size_t start = 0;
    while (start <= all.size()) {
      size_t end = all.find_first_of(";,", start);
      if (end == std::string::npos) end = all.size();
      const std::string item = all.substr(start, end - start);
      const size_t eq = item.find('=');
      if (eq != std::string::npos && KnownKey(NormalizeKey(item.substr(0, eq))))
        ApplyValue(g, NormalizeKey(item.substr(0, eq)), item.substr(eq + 1));
      else if (!Trim(item).empty())
        std::fprintf(stderr, "[gfx] SIMMY_GFX: unknown setting \"%s\"\n", item.c_str());
      start = end + 1;
    }
  }
  for (const char* key : kKeys) {
    std::string name = "SIMMY_GFX_";
    for (const char* p = key; *p; ++p) name += static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    if (!std::strcmp(key, "fpscap")) name = "SIMMY_GFX_FPS_CAP";
    if (const char* env = std::getenv(name.c_str())) ApplyValue(g, key, env);
  }
  if (const char* b = std::getenv("SIMMY_BACKEND")) ApplyValue(g, "backend", b);  // the older switch
  if (const char* s = std::getenv("SIMMY_SPLIT")) ApplyValue(g, "split", s);      // the older switch
  for (const auto& kv : g_cliValues) ApplyValue(g, kv.first, kv.second);
}

void settings_apply_config(AuroraConfig& config, bool offscreen) {
  GraphicsSettings& g = graphics_settings();
  unsigned w = 0, h = 0;
  ConfiguredSize(g, w, h);
  if (w && h) {
    config.windowWidth = w;
    config.windowHeight = h;
    config.surface.width = w;
    config.surface.height = h;
  }
  // AuroraConfig{} leaves the position at 0,0, which puts the window's client area in the top-left
  // corner with its title bar off screen. Open it centred on the main display instead.
  config.windowPosX = SDL_WINDOWPOS_CENTERED;
  config.windowPosY = SDL_WINDOWPOS_CENTERED;
  config.msaa = g.msaa;
  config.maxTextureAnisotropy = static_cast<uint16_t>(g.anisotropy);
  config.vsync = g.vsync;
  config.desiredBackend = static_cast<AuroraBackend>(g.backend);
  config.startFullscreen = !offscreen && g.fullscreen != FullscreenMode::Off;
  g_fullscreenNow = config.startFullscreen;
  const float scale = RenderScale(g);
  char res[48] = "auto (window size)";
  if (scale > 0.0f) std::snprintf(res, sizeof res, "%up (scale %.3g of 640x480)", static_cast<unsigned>(std::lround(480.0f * scale)), scale);
  std::fprintf(stderr,
               "[gfx] %s%s window %ux%u, render %s, aspect %s, hud %s, msaa %u, anisotropy %u, vsync %s, "
               "fps cap %u, backend %s, fullscreen %s\n",
               g.sourceFile.empty() ? "" : g.sourceFile.c_str(), g.sourceFile.empty() ? "" : ":", w, h, res,
               g.wide ? "wide" : "4:3", g.hud == HudMode::Edges ? "edges" : "centered", g.msaa, g.anisotropy,
               g.vsync ? "on" : "off", g.fpsCap, BackendName(g.backend),
               g.fullscreen == FullscreenMode::Off ? "off" : g.fullscreen == FullscreenMode::Borderless ? "borderless" : "exclusive");
}

namespace {
// Tests (software rendering is slow): SIMMY_GFX_LOWRES_UNTIL=N renders at the GameCube's 480 lines
// until frame N, then switches to the configured resolution (also exercising a resize mid-game).
unsigned LowResUntil() {
  static const unsigned n = [] {
    const char* e = std::getenv("SIMMY_GFX_LOWRES_UNTIL");
    return e ? static_cast<unsigned>(std::strtoul(e, nullptr, 10)) : 0u;
  }();
  return n;
}

// Tests: SIMMY_GFX_HIRES_SHOTS=1 renders at the GameCube's 480 lines except for the few frames up to
// each SIMMY_SHOTS frame and the --screenshot frame, which get the configured resolution (each switch
// is a resize, so a long test run costs little more than one at 480 lines).
// SIMMY_GFX_LOWRES_SCALE=<f> (default 1): the scale of 640x480 those low-resolution frames use.
float LowResScale() {
  static const float f = [] {
    const char* e = std::getenv("SIMMY_GFX_LOWRES_SCALE");
    const float v = e ? static_cast<float>(std::atof(e)) : 1.0f;
    return v > 0.0f ? v : 1.0f;
  }();
  return f;
}
bool HiResShots() {
  static const bool on = [] {
    const char* e = std::getenv("SIMMY_GFX_HIRES_SHOTS");
    return e && *e && *e != '0';
  }();
  return on;
}
bool NearShot(unsigned next) {
  static const std::vector<unsigned> shots = [] {
    std::vector<unsigned> out;
    if (const char* env = std::getenv("SIMMY_SHOTS"))
      for (const char* p = env; *p;) {
        out.push_back(static_cast<unsigned>(std::strtoul(p, const_cast<char**>(&p), 10)));
        while (*p == ',' || *p == ' ') ++p;
      }
    return out;
  }();
  const unsigned last = dev_options().screenshotFrame;
  auto near = [next](unsigned s) { return s && next + 3 >= s && next <= s; };
  return near(last) || std::any_of(shots.begin(), shots.end(), near);
}
}  // namespace

void settings_frame_presented(unsigned frame) {
  if (HiResShots()) {
    static bool hi = false;
    const bool want = frame && NearShot(frame + 1);
    if (want != hi) {
      hi = want;
      VISetFrameBufferScale(hi ? RenderScale(graphics_settings()) : LowResScale());
    }
    return;
  }
  if (LowResUntil() && frame == LowResUntil()) {
    const float scale = RenderScale(graphics_settings());
    VISetFrameBufferScale(scale);
    std::fprintf(stderr, "[gfx] frame %u: render scale %.3g\n", frame, scale);
  }
}

void settings_after_init(bool offscreen) {
  g_offscreen = offscreen;
  const GraphicsSettings& g = graphics_settings();
  const float scale = RenderScale(g);
  if (LowResUntil() || HiResShots()) VISetFrameBufferScale(LowResScale());
  else if (scale > 0.0f) VISetFrameBufferScale(scale);
  if (g.areaResampler) aurora_set_resampler(SAMPLER_AREA);
  display_settings_changed();  // the viewport policy for the aspect (pc_splitscreen.cpp)
  if (!offscreen && g.fullscreen == FullscreenMode::Exclusive) SetFullscreen(true);
}

void settings_toggle_fullscreen() {
  if (!aurora_get_window()) return;
  SetFullscreen(!g_fullscreenNow);
}

const char* settings_backend_name(int backend) { return BackendName(backend); }

long long settings_frame_interval_us() {
  const unsigned cap = graphics_settings().fpsCap;
  const unsigned retraces = cap ? std::max(1u, 60u / cap) : 1u;
  return 16667LL * retraces;
}

// ---- In-game options overlay (pc_overlay.cpp) ----

bool settings_fullscreen_now() { return g_fullscreenNow; }

bool settings_set_value(const char* rawKey, const std::string& value) {
  const std::string key = NormalizeKey(rawKey);
  if (!KnownKey(key)) return false;
  ApplyValue(graphics_settings(), key, value);
  return true;
}

void settings_window_size(const GraphicsSettings& g, unsigned& width, unsigned& height) {
  ConfiguredSize(g, width, height);
  if (!width || !height) width = 1280, height = 960;  // aurora's default window
}

bool settings_apply_live(const GraphicsSettings& b) {
  GraphicsSettings& g = graphics_settings();
  bool applied = false;
  const bool aspectChanged = b.wide != g.wide || b.defaultAspect != g.defaultAspect;
  const bool windowChanged =
      b.windowSet != g.windowSet || b.windowWidth != g.windowWidth || b.windowHeight != g.windowHeight;
  unsigned w = 0, h = 0;
  settings_window_size(g, w, h);
  if (aspectChanged || windowChanged) {
    if (g_offscreen) {
      // Tests: the headless surface plays the window.
      aurora_set_surface_size(w, h, aurora_get_window_size().scale);
    } else if (aurora_get_window()) {
      if (!g_fullscreenNow) VISetWindowSize(w, h);
      else if (g.fullscreen == FullscreenMode::Exclusive && windowChanged) SetFullscreen(true);  // the new mode
    }
    applied = true;
  }
  if (aspectChanged) display_settings_changed();
  if (RenderScale(b) != RenderScale(g)) {
    VISetFrameBufferScale(RenderScale(g));  // 0 = auto (the window's size)
    applied = true;
  }
  if (b.areaResampler != g.areaResampler) {
    aurora_set_resampler(g.areaResampler ? SAMPLER_AREA : SAMPLER_BILINEAR);
    applied = true;
  }
  if (b.vsync != g.vsync) {
    aurora_enable_vsync(g.vsync);
    applied = true;
  }
  if (b.anisotropy != g.anisotropy) {
    aurora::webgpu::g_graphicsConfig.textureAnisotropy = static_cast<uint16_t>(std::max(1u, g.anisotropy));
    applied = true;
  }
  if (b.fullscreen != g.fullscreen && !g_offscreen && aurora_get_window()) {
    if (g.fullscreen == FullscreenMode::Off) {
      if (g_fullscreenNow) SetFullscreen(false);
    } else {
      if (g_fullscreenNow) SetFullscreen(false);  // borderless <-> exclusive goes through windowed
      SetFullscreen(true);
    }
    if (g.fullscreen == FullscreenMode::Off) VISetWindowSize(w, h);
    applied = true;
  }
  if (b.hud != g.hud || b.fpsCap != g.fpsCap) applied = true;  // read every frame
  if (applied)
    std::fprintf(stderr, "[gfx] live: aspect %s, hud %s, render %s, window %s, fullscreen %s, vsync %s, fps cap %u, "
                         "anisotropy %u, resampler %s\n",
                 settings_value_string(g, "aspect").c_str(), settings_value_string(g, "hud").c_str(),
                 settings_value_string(g, "resolution").c_str(), settings_value_string(g, "window").c_str(),
                 settings_value_string(g, "fullscreen").c_str(), g.vsync ? "on" : "off", g.fpsCap, g.anisotropy,
                 g.areaResampler ? "area" : "bilinear");
  return applied;
}

std::string settings_value_string(const GraphicsSettings& g, const char* rawKey) {
  const std::string key = NormalizeKey(rawKey);
  char buf[64] = "";
  if (key == "window") {
    if (!g.windowSet) return "auto";
    std::snprintf(buf, sizeof buf, "%ux%u", g.windowWidth, g.windowHeight);
  } else if (key == "fullscreen") {
    return g.fullscreen == FullscreenMode::Off ? "off" : g.fullscreen == FullscreenMode::Borderless ? "borderless" : "exclusive";
  } else if (key == "resolution") {
    if (g.renderScale > 0.0f) std::snprintf(buf, sizeof buf, "%gx", static_cast<double>(g.renderScale));
    else if (g.renderHeight) std::snprintf(buf, sizeof buf, "%up", g.renderHeight);
    else return "auto";
  } else if (key == "aspect") {
    if (!g.wide) return "4:3";
    if (std::fabs(g.defaultAspect - 1.6f) < 0.01f) return "16:10";
    if (std::fabs(g.defaultAspect - 21.0f / 9.0f) < 0.01f) return "21:9";
    return "16:9";
  } else if (key == "hud") {
    return g.hud == HudMode::Edges ? "edges" : "centered";
  } else if (key == "msaa") {
    std::snprintf(buf, sizeof buf, "%u", g.msaa);
  } else if (key == "anisotropy") {
    std::snprintf(buf, sizeof buf, "%u", g.anisotropy);
  } else if (key == "vsync") {
    return g.vsync ? "on" : "off";
  } else if (key == "fpscap") {
    std::snprintf(buf, sizeof buf, "%u", g.fpsCap);
  } else if (key == "backend") {
    return BackendName(g.backend);
  } else if (key == "resampler") {
    return g.areaResampler ? "area" : "bilinear";
  } else if (key == "split") {
    return settings_split_name(g.split);
  } else if (key == "window2") {
    if (!g.window2Set) return "auto";
    std::snprintf(buf, sizeof buf, "%ux%u", g.window2Width, g.window2Height);
  } else if (key == "window2pos") {
    if (!g.window2PosSet) return "auto";
    std::snprintf(buf, sizeof buf, "%d,%d", g.window2X, g.window2Y);
  } else if (key == "window2display") {
    if (g.window2Display < 0) return "auto";
    std::snprintf(buf, sizeof buf, "%d", g.window2Display);
  } else if (key == "fullscreen2") {
    return g.fullscreen2 ? "borderless" : "off";
  } else if (key == "iso") {
    return g.isoPath;
  } else if (key == "p1input") {
    return g.p1Input == 1 ? "controller" : g.p1Input == 2 ? "keyboard_mouse" : "auto";
  } else if (key == "mouseedgepan") {
    return g.mouseEdgePan == 0 ? "off" : g.mouseEdgePan == 1 ? "on" : "fullscreen";
  } else if (key == "mouserotatespeed" || key == "mousezoomspeed") {
    std::snprintf(buf, sizeof buf, "%g", static_cast<double>(key == "mouserotatespeed" ? g.mouseRotateSpeed : g.mouseZoomSpeed));
  } else if (key == "mouseinvertx") {
    return g.mouseInvertX ? "on" : "off";
  } else if (key == "mouseinverty") {
    return g.mouseInvertY ? "on" : "off";
  } else if (key == "mouseinvertwheel") {
    return g.mouseInvertWheel ? "on" : "off";
  }
  return buf;
}

std::string settings_ini_path() {
  const GraphicsSettings& g = graphics_settings();
  if (!g.sourceFile.empty()) return g.sourceFile;
  if (!g_configPath.empty()) return g_configPath;
  if (const char* env = std::getenv("SIMMY_INI")) return env;
  return g_exeDir + "simmy.ini";
}

const char* settings_split_name(int split) {
  switch (split) {
  case 1: return "4x3";
  case 2: return "wide";
  case 3: return "windows";
  default: return "gc";
  }
}

namespace {
// The names written for new lines (the README's spelling).
const char* const kNames[] = {"window", "fullscreen", "resolution", "aspect", "hud", "msaa",
                              "anisotropy", "vsync", "fps_cap", "backend", "resampler", "split",
                              "window2", "window2_pos", "window2_display", "fullscreen2", "iso",
                              "p1_input", "mouse_edge_pan", "mouse_rotate_speed", "mouse_zoom_speed",
                              "mouse_invert_x", "mouse_invert_y", "mouse_invert_wheel"};
bool SaveNames(const char* const* names, size_t count, std::string& message);
}  // namespace

bool settings_save(std::string& message) { return SaveNames(kNames, sizeof kNames / sizeof kNames[0], message); }

bool settings_save_keys(const char* const* keys, size_t count, std::string& message) {
  return SaveNames(keys, count, message);
}

namespace {
bool SaveNames(const char* const* names, size_t count, std::string& message) {
  const GraphicsSettings& g = graphics_settings();
  const GraphicsSettings defaults;
  const std::string path = settings_ini_path();
  std::vector<std::string> lines;
  const bool existed = ReadLines(path, lines);
  if (!existed) {
    lines.push_back("; SIMMY graphics settings (written by the in-game options, F1). See simmy.ini.example.");
    lines.push_back("");
    lines.push_back("[graphics]");
  }
  for (size_t n = 0; n < count; ++n) {
    const char* name = names[n];
    const std::string key = NormalizeKey(name);
    const std::string value = settings_value_string(g, name);
    const bool isDefault = value == settings_value_string(defaults, name);
    const bool isPath = key == "iso";
    if (isPath && value.empty()) continue;  // never blank out the disc the launcher chose
    bool found = false;
    int commented = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
      std::string& line = lines[i];
      const size_t lead = line.find_first_not_of(" \t");
      if (lead == std::string::npos) continue;
      const bool isComment = line[lead] == ';' || line[lead] == '#';
      const size_t keyStart = isComment ? line.find_first_not_of(" \t;#", lead) : lead;
      if (keyStart == std::string::npos) continue;
      const size_t eq = line.find('=', keyStart);
      if (eq == std::string::npos) continue;
      const std::string lineKey = line.substr(keyStart, eq - keyStart);
      if (lineKey.find_first_of(";#") != std::string::npos || NormalizeKey(lineKey) != key) continue;
      if (isComment) {
        // "; aspect = 16:9   <- note" style examples: only a bare "key = value" line counts.
        const std::string rest = Trim(line.substr(eq + 1));
        if (commented < 0 && rest.find_first_of(" \t") == std::string::npos) commented = static_cast<int>(i);
        continue;
      }
      // An active line: replace its value, keeping the key's spelling, the spacing and a trailing comment.
      const size_t valueStart = line.find_first_not_of(" \t", eq + 1);
      size_t comment = isPath ? std::string::npos : line.find_first_of(";#", eq + 1);
      std::string tail;
      if (comment != std::string::npos) {
        size_t cut = comment;
        while (cut > eq + 1 && (line[cut - 1] == ' ' || line[cut - 1] == '\t')) --cut;
        tail = line.substr(cut);
      }
      const std::string pad = valueStart == std::string::npos || (comment != std::string::npos && valueStart >= comment)
                                  ? std::string(" ")
                                  : line.substr(eq + 1, valueStart - eq - 1);
      const bool quoted = isPath && valueStart != std::string::npos && line[valueStart] == '"';  // keep the launcher's quotes
      line = line.substr(0, eq + 1) + pad + (quoted ? "\"" + value + "\"" : value) + tail;
      found = true;
    }
    if (found || isDefault) continue;
    if (commented >= 0) {
      lines[static_cast<size_t>(commented)] = std::string(name) + " = " + value;
    } else {
      // Appended after the last setting line: not inside a [keys] or [launcher] section (the settings
      // reader takes neither as settings), which may follow at the end of the file.
      std::string section;
      int last = -1, firstOther = -1;
      for (size_t i = 0; i < lines.size(); ++i) {
        const std::string t = Trim(lines[i]);
        if (!t.empty() && t[0] == '[') {
          section = Lower(t);
          if ((section == "[keys]" || section == "[launcher]") && firstOther < 0) firstOther = static_cast<int>(i);
          if (section != "[keys]" && section != "[launcher]") last = static_cast<int>(i);
          continue;
        }
        if (!t.empty() && section != "[keys]" && section != "[launcher]") last = static_cast<int>(i);
      }
      const std::string add = std::string(name) + " = " + value;
      if (firstOther < 0) lines.push_back(add);
      else if (last >= 0) lines.insert(lines.begin() + (last + 1), add);
      else lines.insert(lines.begin(), {"[graphics]", add, ""});
    }
  }
  std::string text;
#ifdef _WIN32
  const char* eol = "\r\n";
#else
  const char* eol = "\n";
#endif
  for (const std::string& line : lines) text += line + eol;
  // Write a temporary file next to it, then replace the ini in one step.
  const std::string tmp = path + ".tmp";
  SDL_IOStream* io = SDL_IOFromFile(tmp.c_str(), "wb");
  bool ok = io != nullptr;
  if (io) {
    ok = SDL_WriteIO(io, text.data(), text.size()) == text.size();
    ok = SDL_CloseIO(io) && ok;
  }
  if (ok) ok = SDL_RenamePath(tmp.c_str(), path.c_str());
  if (!ok) {
    message = std::string("Could not write ") + path + ": " + SDL_GetError();
    SDL_RemovePath(tmp.c_str());
    std::fprintf(stderr, "[gfx] %s\n", message.c_str());
    return false;
  }
  graphics_settings().sourceFile = path;
  message = "Saved to " + path;
  std::fprintf(stderr, "[gfx] settings saved to %s\n", path.c_str());
  return true;
}
}  // namespace

}  // namespace simmy
