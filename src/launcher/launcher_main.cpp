// SimmyLauncher: the game launcher (SimmyLauncher.exe next to simmy.exe). It replaces the old
// run_simmy.bat / run_simmy_coop.bat: pick the disc, play alone or in co-op, change the settings in
// simmy.ini, and come back when the game ends (with a friendly report if it crashed).
//
//   SimmyLauncher [--test-iso <file>] [--game-exe <file>]
//   Tests: --screenshot <out.bmp> [--page home|coop|options|playing|error] [--frames N] [--scale S]
//          [--fake-pads N] [--focus N] [--mouse X,Y] [--auto-launch play|coop] [--write-icon <dir>]
//
// What a launch does (the old batch files' behaviour):
//   simmy.exe --iso "<disc>" --split=gc                         Play (one player)
//   simmy.exe --iso "<disc>" --split=<windows|wide|4x3|gc>      Co-op
//     + SIMMY_TWO_PLAYER=1 SIMMY_CAS_ACCEPT=300 SIMMY_SKIP_INTRO=1 with "Quick co-op start"
//   SIMMY_SKIP_INTRO=1 also with "Skip intro movies". The game's output goes to game_out.txt and
//   game_log.txt next to the launcher; the working directory is the launcher's folder.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "art.hpp"
#include "canvas.hpp"
#include "game_process.hpp"
#include "gcm_disc.hpp"
#include "launcher_ini.hpp"
#include "sfx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace launcher {

namespace {

constexpr float kW = 1100.0f, kH = 680.0f;

float Ease(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}
float Approach(float v, float target, float rate) { return v + (target - v) * std::min(1.0f, rate); }

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  return s;
}
std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
  return s.substr(a, b - a);
}
std::string FileName(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}
std::string DirName(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}
bool FileExists(const std::string& path, SDL_Time* modified = nullptr) {
  SDL_PathInfo info;
  if (!SDL_GetPathInfo(path.c_str(), &info) || info.type != SDL_PATHTYPE_FILE) return false;
  if (modified) *modified = info.modify_time;
  return true;
}
std::string Hex(unsigned v) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "0x%08X", v);
  return buf;
}

// ---- settings (simmy.ini) -------------------------------------------------------------------------

struct Choice {
  std::string value, label;
};
struct Setting {
  const char* name;   // simmy.ini spelling
  const char* label;
  const char* help;
  int group;          // 0 display, 1 graphics, 2 game & launcher
  bool launcherKey;   // in [launcher] (the launcher's own), else a [graphics] key the game reads
  std::vector<Choice> choices;
  int defaultIndex = 0;
  int index = 0;
  bool dirty = false;
  bool toggle = false;  // off/on, drawn as a switch
  const std::string& value() const { return choices[static_cast<size_t>(index)].value; }
};

// A value as simmy.ini may spell it, in the spelling the launcher's choices use (the game's aliases).
std::string Canonical(const std::string& key, const std::string& raw) {
  const std::string v = Lower(Trim(raw));
  auto isTrue = [&] { return v == "1" || v == "on" || v == "true" || v == "yes"; };
  auto isFalse = [&] { return v == "0" || v == "off" || v == "false" || v == "no"; };
  if (key == "fullscreen") {
    if (v == "desktop" || isTrue()) return "borderless";
    if (v == "windowed" || isFalse()) return "off";
  } else if (key == "window" || key == "window2") {
    if (v == "default") return "auto";
  } else if (key == "resolution") {
    if (v == "4k") return "2160p";
    if (v == "window" || v == "native") return "auto";
  } else if (key == "aspect") {
    if (v == "gc" || v == "4x3" || v == "original") return "4:3";
    if (v == "16x9" || v == "wide" || v == "widescreen") return "16:9";
    if (v == "16x10") return "16:10";
    if (v == "21x9") return "21:9";
  } else if (key == "hud") {
    if (v == "centred" || v == "center" || v == "centre") return "centered";
    if (v == "edge" || v == "anchored") return "edges";
  } else if (key == "msaa" || key == "anisotropy") {
    if (isFalse()) return "1";
  } else if (key == "vsync" || key == "skipintro" || key == "quickcoop" || key == "sounds") {
    if (isTrue()) return key == "vsync" ? "on" : "1";
    if (isFalse()) return key == "vsync" ? "off" : "0";
  } else if (key == "fpscap") {
    if (v == "off" || v == "unlimited" || v == "uncapped") return "60";
  } else if (key == "backend") {
    if (v == "dx12") return "d3d12";
    if (v == "dx11") return "d3d11";
    if (v == "vk") return "vulkan";
    if (v == "gl") return "opengl";
    if (v == "gles") return "opengles";
    if (v == "default") return "auto";
  } else if (key == "resampler") {
    if (v == "linear") return "bilinear";
    if (v == "box") return "area";
  } else if (key == "p1input") {
    if (v == "both" || v == "default") return "auto";
    if (v == "pad" || v == "gamepad") return "controller";
    if (v == "keyboardmouse" || v == "keyboard" || v == "kbm" || v == "mouse" || v == "keyboard+mouse") return "keyboard_mouse";
  } else if (key == "mouseedgepan") {
    if (v == "auto") return "fullscreen";
    if (isTrue()) return "on";
    if (isFalse()) return "off";
  } else if (key == "coopdisplay" || key == "split") {
    if (v == "original" || v == "4:3") return "gc";
    if (v == "coop4x3") return "4x3";
    if (v == "widescreen") return "wide";
    if (v == "window" || v == "2windows" || v == "two-windows" || v == "dual") return "windows";
  }
  return v;
}

std::vector<Setting> MakeSettings() {
  std::vector<Setting> s;
  auto add = [&](const char* name, const char* label, const char* help, int group, bool launcherKey,
                 std::vector<Choice> choices, int def, bool toggle = false) {
    Setting x{name, label, help, group, launcherKey, std::move(choices)};
    x.defaultIndex = x.index = def;
    x.toggle = toggle;
    s.push_back(std::move(x));
  };
  // Display.
  add("fullscreen", "Screen mode", "Windowed, or fullscreen on your monitor. Alt+Enter switches while you play.", 0, false,
      {{"off", "Windowed"}, {"borderless", "Fullscreen"}, {"exclusive", "Exclusive fullscreen"}}, 0);
  add("window", "Window size", "The game window's size. Automatic picks one that fits the aspect ratio.", 0, false,
      {{"auto", "Automatic"}, {"1280x720", "1280 \xC3\x97 720"}, {"1280x960", "1280 \xC3\x97 960"},
       {"1600x900", "1600 \xC3\x97 900"}, {"1600x1200", "1600 \xC3\x97 1200"}, {"1920x1080", "1920 \xC3\x97 1080"},
       {"1920x1440", "1920 \xC3\x97 1440"}, {"2560x1440", "2560 \xC3\x97 1440"}, {"3840x2160", "3840 \xC3\x97 2160"}},
      0);
  add("aspect", "Aspect ratio", "4:3 is the GameCube's picture. Widescreen shows more of the neighbourhood.", 0, false,
      {{"4:3", "4:3 (original)"}, {"16:9", "16:9 widescreen"}, {"16:10", "16:10 widescreen"}, {"21:9", "21:9 ultrawide"}},
      0);
  add("hud", "HUD in widescreen", "Where the menus and needs panels go on a wide screen (also in co-op windows).", 0,
      false, {{"centered", "Centered"}, {"edges", "Screen edges"}}, 1);
  add("vsync", "V-Sync", "Wait for the monitor's refresh. Smoother scrolling, a little more input delay.", 0, false,
      {{"off", "Off"}, {"on", "On"}}, 0, true);
  add("fps_cap", "Frame rate", "The game runs at up to 60 frames a second; 30 saves power on laptops.", 0, false,
      {{"60", "60 fps"}, {"30", "30 fps"}, {"20", "20 fps"}, {"15", "15 fps"}}, 0);
  // Graphics.
  add("resolution", "Render resolution", "How sharp the 3D picture is drawn. Match window is a good start.", 1, false,
      {{"auto", "Match window"}, {"480p", "480p (GameCube)"}, {"720p", "720p"}, {"1080p", "1080p"}, {"1440p", "1440p"},
       {"2160p", "4K (2160p)"}},
      0);
  add("msaa", "Anti-aliasing", "Smooths jagged edges. Higher costs more GPU time. Applies at the next start.", 1, false,
      {{"1", "Off"}, {"2", "2\xC3\x97 MSAA"}, {"4", "4\xC3\x97 MSAA"}, {"8", "8\xC3\x97 MSAA"}}, 0);
  add("anisotropy", "Texture filtering", "Keeps floors and walls crisp at an angle.", 1, false,
      {{"1", "Off"}, {"2", "2\xC3\x97"}, {"4", "4\xC3\x97"}, {"8", "8\xC3\x97"}, {"16", "16\xC3\x97 (best)"}}, 4);
  add("resampler", "Scaling filter", "How the picture is scaled to the window: smooth, or sharp when rendering above it.",
      1, false, {{"bilinear", "Smooth"}, {"area", "Sharp"}}, 0);
#ifdef _WIN32
  add("backend", "Graphics API", "Automatic picks the best one. Try another if the game shows a black screen.", 1, false,
      {{"auto", "Automatic"}, {"d3d12", "Direct3D 12"}, {"d3d11", "Direct3D 11"}, {"vulkan", "Vulkan"},
       {"opengl", "OpenGL"}},
      0);
#elif defined(__APPLE__)
  add("backend", "Graphics API", "Automatic picks the best one.", 1, false, {{"auto", "Automatic"}, {"metal", "Metal"}}, 0);
#else
  add("backend", "Graphics API", "Automatic picks the best one. Try another if the game shows a black screen.", 1, false,
      {{"auto", "Automatic"}, {"vulkan", "Vulkan"}, {"opengl", "OpenGL"}, {"opengles", "OpenGL ES"}}, 0);
#endif
  // Game, controls and launcher.
  add("p1_input", "Player 1 controls",
      "Both: keyboard + mouse and a controller. Keyboard + mouse: your first controller is player 2's in co-op.",
      2, false,
      {{"auto", "Both"}, {"keyboard_mouse", "Keyboard + mouse"}, {"controller", "Controller"}},
      0);
  add("mouse_edge_pan", "Edge scrolling",
      "With the mouse cursor control, pushing the mouse against a screen edge moves the camera.", 2, false,
      {{"fullscreen", "In fullscreen"}, {"on", "Always"}, {"off", "Off"}}, 0);
  add("skip_intro", "Skip intro movies", "Goes straight from the Maxis screen to the start screen.", 2, true,
      {{"0", "Off"}, {"1", "On"}}, 0, true);
  add("quick_coop", "Quick co-op start",
      "Co-op only: skips the intro, finishes Create-a-Sim on its own and puts both players straight into the house.", 2,
      true, {{"0", "Off"}, {"1", "On"}}, 1, true);
  add("sounds", "Launcher sounds", "The little bloops this launcher makes.", 2, true, {{"0", "Off"}, {"1", "On"}}, 1, true);
  return s;
}

// ---- the app --------------------------------------------------------------------------------------

enum class Page { Home, Coop, Options, Playing };
enum class Style { Green, Blue, Ghost, Orange };
enum class Kind { Button, Cycler, Toggle };

struct Item {
  int id;
  Rect r;
  Kind kind;
};

// Widget ids.
enum : int {
  kIdNone = 0,
  kIdChooseDisc = 100, kIdPlay, kIdCoop, kIdOptions, kIdQuit,
  kIdLayout0 = 200,  // +split
  kIdQuickCoop = 210, kIdCoopBack, kIdCoopStart,
  kIdSetting0 = 300,  // +setting index
  kIdOptionsBack = 400, kIdOptionsReset,
  kIdForceQuit = 500,
  kIdErrorFolder = 900, kIdErrorOk,
};

struct Toast {
  std::string text;
  float time = 0;
};

struct ErrorReport {
  bool open = false;
  std::string title, body, excerpt;
  bool offerFolder = true;
};

struct Options {
  std::string screenshot;
  std::string page = "home";
  int frames = 45;
  float scale = 0;
  int fakePads = -1;
  int focus = 0;
  float mouseX = -1, mouseY = -1;
  std::string testIso, gameExe, autoLaunch, writeIcon;
  std::string choose;               // as if picked in the file dialog (remembered in simmy.ini)
  std::vector<std::string> sets;    // name=value, as if changed on the Options page
};

Uint32 g_dialogEvent = 0;

void SDLCALL DialogCallback(void*, const char* const* files, int) {
  SDL_Event e;
  SDL_zero(e);
  e.type = g_dialogEvent;
  if (!files) {
    e.user.code = 2;  // error
    e.user.data1 = SDL_strdup(SDL_GetError());
  } else if (!files[0]) {
    e.user.code = 1;  // cancelled
  } else {
    e.user.code = 0;
    e.user.data1 = SDL_strdup(files[0]);
  }
  SDL_PushEvent(&e);
}

class App {
 public:
  explicit App(const Options& o) : opt_(o) {}
  int run();

 private:
  // Setup.
  bool createWindow();
  void applyScale(float scale);
  float pickScale() const;
  void loadIni();
  void saveSettings(bool quiet);
  void setDisc(const std::string& path, bool remember, bool fromUser);
  void openDiscDialog();

  // Input.
  void handleEvent(SDL_Event& e);
  void navigate(int dx, int dy);
  void back();
  void heldRepeat(float dt);
  void openPads();
  void closePads();

  // Launching.
  void launch(bool coop);
  void pollGame();
  void showError(const std::string& title, const std::string& body, const std::string& excerpt, bool folder);

  // Drawing.
  void frame(float dt);
  void drawHome();
  void drawCoop();
  void drawOptions();
  void drawPlaying();
  void drawError();
  void drawToast();
  void drawHints();
  void drawHeader(const std::string& title, const std::string& subtitle);
  void panel(Rect r, const std::string& title);
  void chip(float x, float y, const std::string& text, Color bg, Color fg, float* width = nullptr);
  bool button(int id, Rect r, const std::string& label, Style style, Icon icon, bool enabled = true,
              FontId font = kFontButton, const std::string& sub = std::string());
  void settingRow(int id, Rect r, Setting& s);
  bool layoutCard(int id, Rect r, int split, const char* title, const char* help, bool selected);
  void reg(int id, Rect r, Kind kind);
  float hover(int id);
  void setPage(Page p, int focus);
  bool screenshotNow(const std::string& path);

  Setting* setting(const char* name) {
    for (Setting& s : settings_)
      if (!std::strcmp(s.name, name)) return &s;
    return nullptr;
  }
  bool flag(const char* name) {
    Setting* s = setting(name);
    return s && s->value() == "1";
  }

  Options opt_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Surface* surface_ = nullptr;  // tests without a video driver
  Canvas canvas_;
  float scale_ = 1.0f;
  bool quit_ = false;
  float time_ = 0;

  std::string baseDir_, iniPath_, gameDir_;
  IniFile ini_;
  std::vector<Setting> settings_ = MakeSettings();
  std::string coopDisplay_ = "windows";
  DiscInfo disc_;
  bool haveDisc_ = false;  // a disc was chosen (valid or not)
  bool dialogOpen_ = false;
  SDL_Texture* banner_ = nullptr;

  Page page_ = Page::Home;
  float pageTime_ = 0;
  std::vector<Item> items_, prevItems_;
  int focus_ = kIdChooseDisc;
  int activated_ = 0, pendingActivate_ = 0;
  int cycle_ = 0, pendingCycle_ = 0;
  bool pendingBack_ = false;
  float mx_ = -1000, my_ = -1000;
  float clickX_ = -1;
  bool mouseDown_ = false, mouseMoved_ = false;
  int pressId_ = 0;
  int flashId_ = 0;
  float flashTime_ = 0;
  std::unordered_map<int, float> hoverAnim_, pressAnim_, toggleAnim_;
  int heldDx_ = 0, heldDy_ = 0;
  float heldTime_ = 0, heldNext_ = 0;
  int stickDx_ = 0, stickDy_ = 0;
  bool blocked_ = false;  // a modal is open: page widgets are drawn but inactive

  std::vector<SDL_Gamepad*> pads_;
  bool padsActive_ = true;

  GameProcess game_;
  bool coopRunning_ = false;
  SDL_Time launchTime_ = 0;
  bool forceQuit_ = false;  // the player pressed "Force quit": its exit code is not a failure
  std::string runningMode_;
  ErrorReport error_;
  Toast toast_;
  int autoLaunchFrame_ = -1;
  int afterGameFrames_ = -1;
  bool minimized_ = false;
};

// ---- setup ----------------------------------------------------------------------------------------

float App::pickScale() const {
  if (opt_.scale > 0) return opt_.scale;
  if (!window_) return 1.0f;
  // Windows sizes windows in pixels (SDL is per-monitor DPI aware), macOS / Wayland in points with a
  // denser back buffer; either way the layout grows with the display scale, but must fit the screen.
  const float density = std::max(1.0f, SDL_GetWindowPixelDensity(window_));
  float display = SDL_GetWindowDisplayScale(window_);
  if (!(display > 0)) display = 1.0f;
  float ui = std::max(display, density);
  SDL_Rect usable;
  if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window_), &usable) && usable.w > 0 && usable.h > 0) {
    const float perPoint = ui / density;  // window units per logical pixel
    const float fit = std::min(usable.w * 0.96f / kW, (usable.h - 48 * display / density) * 0.96f / kH);
    if (perPoint > fit) ui = std::max(0.5f, fit) * density;
  }
  return ui;
}

void App::applyScale(float scale) {
  scale_ = scale;
  if (window_) {
    const float density = std::max(1.0f, SDL_GetWindowPixelDensity(window_));
    SDL_SetWindowSize(window_, static_cast<int>(std::lround(kW * scale / density)),
                      static_cast<int>(std::lround(kH * scale / density)));
    SDL_SyncWindow(window_);
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(window_, &pw, &ph);
    if (pw > 0) scale_ = static_cast<float>(pw) / kW;
  }
  canvas_.init(renderer_, scale_);
}

bool App::createWindow() {
  const bool test = !opt_.screenshot.empty() || !opt_.writeIcon.empty();
  SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    if (!test) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SIMMY", SDL_GetError(), nullptr);
      return false;
    }
    std::fprintf(stderr, "[launcher] no video (%s); drawing into a surface\n", SDL_GetError());
  }
  if (SDL_WasInit(SDL_INIT_VIDEO)) {
    window_ = SDL_CreateWindow("SIMMY", static_cast<int>(kW), static_cast<int>(kH), SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (window_) renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_ && window_) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
    }
  }
  if (!renderer_) {
    if (!test) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SIMMY", SDL_GetError(), nullptr);
      return false;
    }
    const float s = opt_.scale > 0 ? opt_.scale : 1.0f;
    surface_ = SDL_CreateSurface(static_cast<int>(kW * s), static_cast<int>(kH * s), SDL_PIXELFORMAT_ARGB8888);
    renderer_ = surface_ ? SDL_CreateSoftwareRenderer(surface_) : nullptr;
    if (!renderer_) return false;
  }
  SDL_SetRenderVSync(renderer_, 1);
  applyScale(pickScale());
  if (!surface_ && opt_.scale <= 0) {
    // The scale can only be known once the window is on a display; settle it once more.
    const float again = pickScale();
    if (std::fabs(again - scale_) > 0.01f) applyScale(again);
  }
  if (window_) {
    // The window icon: a plumbob drawn into a surface.
    SDL_Surface* icon = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_ARGB8888);
    if (icon) {
      SDL_Renderer* sw = SDL_CreateSoftwareRenderer(icon);
      if (sw) {
        SDL_SetRenderDrawColor(sw, 0, 0, 0, 0);
        SDL_RenderClear(sw);
        Canvas ic;
        ic.init(sw, 1.0f, false);
        draw_plumbob(ic, 32, 27, 50, 0.35f, 0.0f, false);
        ic.flush();
        SDL_RenderPresent(sw);
        SDL_DestroyRenderer(sw);
        SDL_SetWindowIcon(window_, icon);
      }
      SDL_DestroySurface(icon);
    }
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window_);
  }
  return true;
}

void App::loadIni() {
  ini_.load(iniPath_);
  for (Setting& s : settings_) {
    const std::string key = IniFile::normalizeKey(s.name);
    const std::string raw = s.launcherKey ? ini_.getLauncher(s.name) : ini_.get(s.name);
    s.dirty = false;
    // Drop a custom choice added by an earlier load.
    s.choices.erase(std::remove_if(s.choices.begin(), s.choices.end(),
                                   [](const Choice& c) { return c.label.size() > 9 && c.label.compare(c.label.size() - 9, 9, " (custom)") == 0; }),
                    s.choices.end());
    s.index = s.defaultIndex;
    if (raw.empty()) continue;
    const std::string v = Canonical(key, raw);
    bool found = false;
    for (size_t i = 0; i < s.choices.size(); ++i)
      if (s.choices[i].value == v) s.index = static_cast<int>(i), found = true;
    if (!found) {
      // A value the launcher has no button for (hand-edited): keep it as an extra choice.
      s.choices.insert(s.choices.begin(), Choice{raw, raw + " (custom)"});
      ++s.defaultIndex;
      s.index = 0;
    }
  }
  sfx_enable(flag("sounds"));
  const std::string coop = Canonical("coopdisplay", ini_.getLauncher("coop_display"));
  if (coop == "gc" || coop == "4x3" || coop == "wide" || coop == "windows") {
    coopDisplay_ = coop;
  } else {
    const std::string split = Canonical("split", ini_.get("split"));
    if (split == "4x3" || split == "wide" || split == "windows") coopDisplay_ = split;
  }
}

void App::saveSettings(bool quiet) {
  bool any = false;
  for (Setting& s : settings_) {
    if (!s.dirty) continue;
    if (s.launcherKey) ini_.setLauncher(s.name, s.value());
    else ini_.setGraphics(s.name, s.value(), s.index == s.defaultIndex);
    s.dirty = false;
    any = true;
  }
  if (!any) return;
  std::string message;
  if (!ini_.save(message)) toast_ = Toast{message, 0};
  else if (!quiet) toast_ = Toast{"Settings saved to simmy.ini", 0};
}

void App::setDisc(const std::string& path, bool remember, bool fromUser) {
  disc_ = probe_disc(path);
  haveDisc_ = true;
  if (banner_) SDL_DestroyTexture(banner_);
  banner_ = nullptr;
  if (disc_.hasBanner) {
    banner_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, kBannerWidth, kBannerHeight);
    if (banner_) {
      SDL_UpdateTexture(banner_, nullptr, disc_.bannerRgba.data(), kBannerWidth * 4);
      SDL_SetTextureBlendMode(banner_, SDL_BLENDMODE_BLEND);
      SDL_SetTextureScaleMode(banner_, SDL_SCALEMODE_PIXELART);
    }
  }
  std::fprintf(stderr, "[launcher] disc %s: %s %s \"%s\" %s\n", path.c_str(), disc_.ok() ? "ok" : "rejected",
               disc_.gameId.c_str(), disc_.displayTitle().c_str(), disc_.message.c_str());
  if (!fromUser) return;
  if (disc_.ok()) {
    sfx_play(Sfx::Disc);
    if (remember) {
      ini_.load(iniPath_);
      ini_.setTopLevel("iso", path, "The Sims 2 GameCube disc image (chosen in SimmyLauncher; also simmy.exe --iso <file>).");
      std::string message;
      toast_ = Toast{ini_.save(message) ? "Disc remembered. Have fun!" : message, 0};
    }
    focus_ = kIdPlay;
  } else {
    sfx_play(Sfx::Error);
    focus_ = kIdChooseDisc;
  }
}

void App::openDiscDialog() {
  if (dialogOpen_ || !window_) return;
  static const SDL_DialogFileFilter kFilters[] = {{"GameCube disc image (*.iso, *.gcm)", "iso;gcm"}, {"All files", "*"}};
  dialogOpen_ = true;
  std::string where = disc_.path.empty() ? std::string() : DirName(disc_.path);
  SDL_ShowOpenFileDialog(&DialogCallback, nullptr, window_, kFilters, 2, where.empty() ? nullptr : where.c_str(), false);
}

// ---- input ----------------------------------------------------------------------------------------

void App::openPads() {
  if (!SDL_WasInit(SDL_INIT_GAMEPAD) && !SDL_InitSubSystem(SDL_INIT_GAMEPAD)) return;
  padsActive_ = true;
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  for (int i = 0; ids && i < count; ++i) {
    bool have = false;
    for (SDL_Gamepad* p : pads_) have = have || SDL_GetGamepadID(p) == ids[i];
    if (!have)
      if (SDL_Gamepad* p = SDL_OpenGamepad(ids[i])) pads_.push_back(p);
  }
  SDL_free(ids);
}

void App::closePads() {
  // While the game runs the controllers are all the game's.
  for (SDL_Gamepad* p : pads_) SDL_CloseGamepad(p);
  pads_.clear();
  if (SDL_WasInit(SDL_INIT_GAMEPAD)) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
  padsActive_ = false;
}

void App::navigate(int dx, int dy) {
  const std::vector<Item>& items = prevItems_;
  const Item* cur = nullptr;
  for (const Item& it : items)
    if (it.id == focus_) cur = &it;
  if (!cur) {
    if (!items.empty()) focus_ = items.front().id, sfx_play(Sfx::Hover);
    return;
  }
  if (dx != 0 && (cur->kind == Kind::Cycler || cur->kind == Kind::Toggle)) {
    pendingCycle_ = dx;
    return;
  }
  const Item* best = nullptr;
  float bestScore = 1e9f;
  for (const Item& it : items) {
    if (it.id == cur->id) continue;
    const float vx = it.r.cx() - cur->r.cx(), vy = it.r.cy() - cur->r.cy();
    const float along = dx ? vx * static_cast<float>(dx) : vy * static_cast<float>(dy);
    if (along <= 4) continue;
    float across = dx ? std::fabs(vy) : std::fabs(vx);
    // Overlapping in the other axis counts as straight ahead.
    const bool overlap = dx ? (it.r.y < cur->r.y + cur->r.h && cur->r.y < it.r.y + it.r.h)
                            : (it.r.x < cur->r.x + cur->r.w && cur->r.x < it.r.x + it.r.w);
    if (overlap) across *= 0.15f;
    const float score = along + across * 2.2f;
    if (score < bestScore) bestScore = score, best = &it;
  }
  if (best) {
    focus_ = best->id;
    sfx_play(Sfx::Hover);
  }
}

void App::back() {
  if (error_.open) {
    error_.open = false;
    sfx_play(Sfx::Back);
    return;
  }
  if (page_ == Page::Coop || page_ == Page::Options) {
    if (page_ == Page::Options) saveSettings(false);
    sfx_play(Sfx::Back);
    setPage(Page::Home, page_ == Page::Coop ? kIdCoop : kIdOptions);
  }
}

void App::heldRepeat(float dt) {
  const int dx = heldDx_ ? heldDx_ : stickDx_, dy = heldDy_ ? heldDy_ : stickDy_;
  if (!dx && !dy) {
    heldTime_ = 0;
    heldNext_ = 0;
    return;
  }
  heldTime_ += dt;
  if (heldTime_ >= heldNext_) {
    navigate(dx, dy);
    heldNext_ = heldNext_ == 0 ? 0.42f : heldNext_ + 0.13f;
  }
}

void App::handleEvent(SDL_Event& e) {
  if (e.type == g_dialogEvent && g_dialogEvent) {
    dialogOpen_ = false;
    char* text = static_cast<char*>(e.user.data1);
    if (e.user.code == 0 && text) setDisc(text, true, true);
    else if (e.user.code == 2) toast_ = Toast{std::string("Couldn't open the file picker: ") + (text ? text : ""), 0};
    SDL_free(text);
    return;
  }
  switch (e.type) {
  case SDL_EVENT_QUIT: quit_ = true; break;
  case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    if (opt_.scale <= 0 && std::fabs(pickScale() - scale_) > 0.01f) applyScale(pickScale());
    break;
  case SDL_EVENT_WINDOW_MINIMIZED: minimized_ = true; break;
  case SDL_EVENT_WINDOW_RESTORED:
  case SDL_EVENT_WINDOW_SHOWN: minimized_ = false; break;
  case SDL_EVENT_MOUSE_MOTION:
    SDL_ConvertEventToRenderCoordinates(renderer_, &e);
    mx_ = e.motion.x / scale_, my_ = e.motion.y / scale_;
    mouseMoved_ = true;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (e.button.button != SDL_BUTTON_LEFT) {
      if (e.button.button == SDL_BUTTON_X1) pendingBack_ = true;
      break;
    }
    SDL_ConvertEventToRenderCoordinates(renderer_, &e);
    mx_ = e.button.x / scale_, my_ = e.button.y / scale_;
    mouseDown_ = true;
    pressId_ = 0;
    for (const Item& it : prevItems_)
      if (it.r.contains(mx_, my_)) pressId_ = it.id;
    break;
  case SDL_EVENT_MOUSE_BUTTON_UP:
    if (e.button.button != SDL_BUTTON_LEFT) break;
    SDL_ConvertEventToRenderCoordinates(renderer_, &e);
    mx_ = e.button.x / scale_, my_ = e.button.y / scale_;
    mouseDown_ = false;
    for (const Item& it : prevItems_)
      if (it.r.contains(mx_, my_) && it.id == pressId_) {
        pendingActivate_ = it.id;
        clickX_ = mx_;
        focus_ = it.id;
      }
    pressId_ = 0;
    break;
  case SDL_EVENT_KEY_DOWN:
    switch (e.key.key) {
    case SDLK_UP: navigate(0, -1); break;
    case SDLK_DOWN: navigate(0, 1); break;
    case SDLK_LEFT: navigate(-1, 0); break;
    case SDLK_RIGHT: navigate(1, 0); break;
    case SDLK_TAB: {
      // Tab / Shift+Tab: the next / previous widget in drawing order.
      if (prevItems_.empty()) break;
      size_t at = 0;
      for (size_t i = 0; i < prevItems_.size(); ++i)
        if (prevItems_[i].id == focus_) at = i;
      const size_t n = prevItems_.size();
      at = (e.key.mod & SDL_KMOD_SHIFT) ? (at + n - 1) % n : (at + 1) % n;
      focus_ = prevItems_[at].id;
      sfx_play(Sfx::Hover);
      break;
    }
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE:
      if (!e.key.repeat) pendingActivate_ = focus_, clickX_ = -1;
      break;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE:
      if (!e.key.repeat) pendingBack_ = true;
      break;
    default: break;
    }
    break;
  case SDL_EVENT_GAMEPAD_ADDED:
    if (padsActive_)
      if (SDL_Gamepad* p = SDL_OpenGamepad(e.gdevice.which)) {
        bool have = false;
        for (SDL_Gamepad* q : pads_) have = have || q == p;
        if (!have) pads_.push_back(p);
      }
    break;
  case SDL_EVENT_GAMEPAD_REMOVED:
    for (size_t i = 0; i < pads_.size(); ++i)
      if (SDL_GetGamepadID(pads_[i]) == e.gdevice.which) {
        SDL_CloseGamepad(pads_[i]);
        pads_.erase(pads_.begin() + static_cast<std::ptrdiff_t>(i));
        break;
      }
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    switch (e.gbutton.button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: heldDx_ = 0, heldDy_ = -1, heldTime_ = heldNext_ = 0; break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: heldDx_ = 0, heldDy_ = 1, heldTime_ = heldNext_ = 0; break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: heldDx_ = -1, heldDy_ = 0, heldTime_ = heldNext_ = 0; break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: heldDx_ = 1, heldDy_ = 0, heldTime_ = heldNext_ = 0; break;
    case SDL_GAMEPAD_BUTTON_SOUTH:
    case SDL_GAMEPAD_BUTTON_START: pendingActivate_ = focus_, clickX_ = -1; break;
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_BACK: pendingBack_ = true; break;
    default: break;
    }
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_UP:
    if (e.gbutton.button >= SDL_GAMEPAD_BUTTON_DPAD_UP && e.gbutton.button <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
      heldDx_ = heldDy_ = 0;
    break;
  case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
    if (e.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTX && e.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTY) break;
    const float v = static_cast<float>(e.gaxis.value) / 32767.0f;
    int& d = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX ? stickDx_ : stickDy_;
    const int was = d;
    if (std::fabs(v) > 0.55f) d = v > 0 ? 1 : -1;
    else if (std::fabs(v) < 0.35f) d = 0;
    if (d != was) heldTime_ = heldNext_ = 0;
    if (stickDx_ && stickDy_) {  // diagonal: the axis that just moved wins
      if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) stickDy_ = 0;
      else stickDx_ = 0;
    }
    break;
  }
  case SDL_EVENT_DROP_FILE:
    if (e.drop.data && page_ != Page::Playing) setDisc(e.drop.data, true, true);
    break;
  default: break;
  }
}

// ---- launching ------------------------------------------------------------------------------------

void App::showError(const std::string& title, const std::string& body, const std::string& excerpt, bool folder) {
  if (afterGameFrames_ < 0) afterGameFrames_ = 0;  // tests (--auto-launch): a failed start ends the run too
  error_.open = true;
  error_.title = title;
  error_.body = body;
  error_.excerpt = excerpt;
  error_.offerFolder = folder;
  focus_ = folder ? kIdErrorFolder : kIdErrorOk;
  sfx_play(Sfx::Error);
}

void App::launch(bool coop) {
  if (!disc_.ok() || game_.running()) return;
  saveSettings(true);
  LaunchRequest r;
#ifdef _WIN32
  r.exe = baseDir_ + "simmy.exe";
#else
  r.exe = baseDir_ + "simmy";
#endif
  if (!opt_.gameExe.empty()) r.exe = opt_.gameExe;
  if (!FileExists(r.exe)) {
    showError("Can't find the game", "SIMMY couldn't find " + FileName(r.exe) +
                                         " next to the launcher. Build it with build_simmy.bat, then start the launcher from the same folder.",
              r.exe, true);
    return;
  }
  r.args = {"--iso", disc_.path};
  const bool quick = coop && flag("quick_coop");
  r.args.push_back(std::string("--split=") + (coop ? coopDisplay_ : std::string("gc")));
  r.workingDir = gameDir_ = DirName(r.exe);
  r.stdoutPath = baseDir_ + "game_out.txt";
  r.stderrPath = baseDir_ + "game_log.txt";
  r.env = {{"SIMMY_SKIP_INTRO", (flag("skip_intro") || quick) ? "1" : ""},
           {"SIMMY_TWO_PLAYER", quick ? "1" : ""},
           {"SIMMY_CAS_ACCEPT", quick ? "300" : ""}};
  // Test and diagnostic switches (SIMMY_WATCHDOG, SIMMY_DET*, SIMMY_TEST_*, SIMMY_SHOTS, ...) left in
  // the launcher's environment, e.g. by a build script, must not reach a normal game: every other
  // SIMMY_* variable is removed from the game's environment, except SIMMY_INI (the launcher and the game
  // read the same simmy.ini). SIMMY_LAUNCHER_KEEP_ENV=1 passes them through (development).
  std::string stripped;
  const char* keep = std::getenv("SIMMY_LAUNCHER_KEEP_ENV");
  if (!(keep && *keep && *keep != '0')) {
    for (const std::string& name : inherited_simmy_variables()) {
      if (name == "SIMMY_INI" || name == "SIMMY_LAUNCHER_KEEP_ENV") continue;
      bool set = false;
      for (const auto& kv : r.env) set = set || kv.first == name;
      if (set) continue;
      r.env.emplace_back(name, "");
      stripped += " " + name;
    }
  }
  SDL_GetCurrentTime(&launchTime_);
  launchTime_ -= SDL_NS_PER_SECOND * 2;  // file times can be coarse
  std::string err;
  const std::string command = describe_command(r);
  std::fprintf(stderr, "[launcher] starting: %s\n", command.c_str());
  if (SDL_IOStream* log = SDL_IOFromFile((baseDir_ + "simmy_launcher_log.txt").c_str(), "ab")) {
    char when[64] = "";
    const std::time_t now = std::time(nullptr);
    std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    SDL_IOprintf(log, "%s start: %s\n", when, command.c_str());
    if (!stripped.empty()) SDL_IOprintf(log, "    removed from the game's environment:%s\n", stripped.c_str());
    SDL_CloseIO(log);
  }
  closePads();
  forceQuit_ = false;
  if (!game_.start(r, err)) {
    openPads();
    showError("The game didn't start", err, "", true);
    return;
  }
  coopRunning_ = coop;
  runningMode_ = coop ? std::string("Co-op \xC2\xB7 ") +
                            (coopDisplay_ == "windows" ? "Two windows"
                             : coopDisplay_ == "wide"  ? "One wide window"
                             : coopDisplay_ == "4x3"   ? "Two 4:3 halves"
                                                       : "GameCube split")
                      : std::string("One player");
  sfx_play(Sfx::Launch);
  setPage(Page::Playing, kIdForceQuit);
  if (window_) {
    SDL_MinimizeWindow(window_);
    std::fprintf(stderr, "[launcher] window minimized while the game runs\n");
  }
}

void App::pollGame() {
  int code = 0;
  std::string description;
  if (!game_.poll(code, description)) return;
  std::fprintf(stderr, "[launcher] game exited with code %d (%s)\n", code, Hex(static_cast<unsigned>(code)).c_str());
  // A crash report the game wrote next to itself during this run (pc_crash_win.cpp). The watchdog's
  // timed snapshot (simmy_watchdog.txt, SIMMY_WATCHDOG) is a diagnostic, not a failure: it is ignored.
  std::string report, reportName;
  {
    SDL_Time t = 0;
    const std::string path = gameDir_ + "simmy_crash.txt";
    if (FileExists(path, &t) && t >= launchTime_) {
      size_t size = 0;
      if (void* data = SDL_LoadFile(path.c_str(), &size)) {
        report.assign(static_cast<const char*>(data), std::min<size_t>(size, 4000));
        reportName = "simmy_crash.txt";
        SDL_free(data);
      }
    }
  }
  SDL_Time dumpTime = 0;
  const bool haveDump = FileExists(gameDir_ + "simmy_crash.dmp", &dumpTime) && dumpTime >= launchTime_;
  if (SDL_IOStream* log = SDL_IOFromFile((baseDir_ + "simmy_launcher_log.txt").c_str(), "ab")) {
    SDL_IOprintf(log, "    exit code %d (%s) %s%s\n", code, Hex(static_cast<unsigned>(code)).c_str(), description.c_str(),
                 forceQuit_ ? " (force quit)" : "");
    if (!report.empty()) {
      SDL_IOprintf(log, "    crash report %s%s%s:\n", (gameDir_ + reportName).c_str(), haveDump ? ", minidump " : "",
                   haveDump ? (gameDir_ + "simmy_crash.dmp").c_str() : "");
      size_t start = 0;
      for (int n = 0; n < 40 && start < report.size(); ++n) {
        size_t end = report.find('\n', start);
        if (end == std::string::npos) end = report.size();
        std::string l = report.substr(start, end - start);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (!l.empty()) SDL_IOprintf(log, "      %s\n", l.c_str());
        start = end + 1;
      }
    }
    SDL_CloseIO(log);
  }
  if (window_) {
    SDL_RestoreWindow(window_);
    SDL_RaiseWindow(window_);
    std::fprintf(stderr, "[launcher] window restored\n");
  }
  openPads();
  loadIni();  // the game's F1 options may have saved changes
  setPage(Page::Home, kIdPlay);
  afterGameFrames_ = 0;

  // A clean exit (code 0, no crash report) or the player's own "Force quit": no message.
  if (report.empty() && (code == 0 || forceQuit_)) return;

  // The excerpt: the report's first lines, else the end of the game's log.
  std::string excerpt;
  auto lines = [](const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < text.size()) {
      size_t end = text.find('\n', start);
      if (end == std::string::npos) end = text.size();
      std::string l = text.substr(start, end - start);
      if (!l.empty() && l.back() == '\r') l.pop_back();
      if (!Trim(l).empty()) out.push_back(l);
      start = end + 1;
    }
    return out;
  };
  if (!report.empty()) {
    const std::vector<std::string> l = lines(report);
    for (size_t i = 0; i < l.size() && i < 6; ++i) excerpt += l[i] + "\n";
  } else {
    size_t size = 0;
    if (void* data = SDL_LoadFile((baseDir_ + "game_log.txt").c_str(), &size)) {
      const std::string text(static_cast<const char*>(data), size);
      SDL_free(data);
      const std::vector<std::string> l = lines(text);
      for (size_t i = l.size() > 6 ? l.size() - 6 : 0; i < l.size(); ++i) excerpt += l[i] + "\n";
    }
  }
  std::string body;
  if (!report.empty()) body = "The game crashed and wrote a crash report.";
  else if (!description.empty()) body = "The game stopped unexpectedly: " + description + ".";
  else body = "The game stopped unexpectedly.";
  body += " Exit code " + (code >= 0 && code < 256 ? std::to_string(code) : Hex(static_cast<unsigned>(code))) + ".";
  std::string files = "game_log.txt, game_out.txt";
  if (!report.empty()) files += ", simmy_crash.txt";
  if (haveDump) files += ", simmy_crash.dmp";
  body += " Please send these files with your report: " + files + " (the button below opens their folder).";
  if (gameDir_ != baseDir_ && (!report.empty() || haveDump)) body += " The crash files are in " + gameDir_ + ".";
  showError("Uh-oh! Something went wrong", body, excerpt, true);
}

// ---- widgets --------------------------------------------------------------------------------------

void App::reg(int id, Rect r, Kind kind) {
  if (blocked_) return;
  items_.push_back(Item{id, r, kind});
  if (mouseMoved_ && r.contains(mx_, my_) && focus_ != id) {
    focus_ = id;
    sfx_play(Sfx::Hover);
  }
}

float App::hover(int id) {
  float& v = hoverAnim_[id];
  return v = Approach(v, !blocked_ && focus_ == id ? 1.0f : 0.0f, 0.25f);
}

struct Palette {
  Color top, bottom, border, glow, textShadow;
};
Palette StylePalette(Style s, bool enabled) {
  if (!enabled) return Palette{rgb(0x9DB2CB), rgb(0x6F86A5), rgb(0xFFFFFF, 0.35f), rgb(0xFFFFFF), rgb(0x2A4466, 0.45f)};
  switch (s) {
  case Style::Green: return Palette{rgb(0xA4EE62), rgb(0x34A126), rgb(0xE2FFC0), rgb(0xC6FF8A), rgb(0x1A5A10, 0.55f)};
  case Style::Orange: return Palette{rgb(0xFFCB74), rgb(0xEE8420), rgb(0xFFEBC6), rgb(0xFFD79A), rgb(0x7A3A06, 0.5f)};
  case Style::Ghost:
    return Palette{rgb(0x2D6FC4, 0.62f), rgb(0x123F86, 0.72f), rgb(0xFFFFFF, 0.6f), rgb(0xFFFFFF), rgb(0x0B3470, 0.55f)};
  case Style::Blue:
  default: return Palette{rgb(0x63BDFF), rgb(0x216CD4), rgb(0xC7E8FF), rgb(0xB0DEFF), rgb(0x0B3A82, 0.55f)};
  }
}

bool App::button(int id, Rect r, const std::string& label, Style style, Icon icon, bool enabled, FontId font,
                 const std::string& sub) {
  if (enabled) reg(id, r, Kind::Button);
  const float hv = enabled ? hover(id) : 0.0f;
  float pr = 0;
  {
    float& v = pressAnim_[id];
    const bool down = enabled && !blocked_ &&
                      ((mouseDown_ && pressId_ == id && r.contains(mx_, my_)) || (flashId_ == id && flashTime_ > 0));
    pr = v = Approach(v, down ? 1.0f : 0.0f, 0.45f);
  }
  const Palette p = StylePalette(style, enabled);
  const float radius = std::min(r.h * 0.5f, 30.0f);
  canvas_.pushZoom(r.cx(), r.cy(), 1.0f + 0.03f * hv - 0.04f * pr);
  canvas_.roundRectGlow(Rect{r.x + 2, r.y + 6, r.w - 4, r.h}, radius, 14, rgb(0x062457, enabled ? 0.30f : 0.15f));
  if (hv > 0.01f) canvas_.roundRectGlow(r, radius, 18, withAlpha(p.glow, 0.6f * hv));
  const Color top = mix(p.top, rgb(0xFFFFFF, p.top.a), 0.10f * hv), bottom = mix(p.bottom, rgb(0xFFFFFF, p.bottom.a), 0.08f * hv);
  canvas_.roundRect(r, radius, mix(top, bottom, 0.25f * pr), bottom);
  // Gloss on the upper half.
  canvas_.roundRect(Rect{r.x + 4, r.y + 3, r.w - 8, r.h * 0.48f}, std::max(2.0f, radius - 3), rgb(0xFFFFFF, enabled ? 0.38f : 0.2f),
                    rgb(0xFFFFFF, 0.04f));
  canvas_.roundRectStroke(r, radius, 1.6f, withAlpha(p.border, 0.9f));
  const Color text = rgb(0xFFFFFF, enabled ? 1.0f : 0.8f);
  if (!sub.empty()) {
    // Big button: an icon in a bubble on the left, the label and a sub-line.
    const float bub = r.h * 0.62f;
    const Vec2 bc{r.x + r.h * 0.55f, r.cy()};
    canvas_.circle(bc, bub * 0.5f, rgb(0xFFFFFF, 0.22f));
    canvas_.ring(bc, bub * 0.5f, 1.5f, rgb(0xFFFFFF, 0.45f));
    draw_icon(canvas_, icon, bc, bub * 0.52f, text);
    const float tx = r.x + r.h * 1.05f;
    canvas_.textShadow(font, tx, r.cy() + 2, label, text, p.textShadow, Align::Left, 2.0f);
    canvas_.textShadow(kFontSmall, tx + 1, r.cy() + 24, sub, rgb(0xFFFFFF, enabled ? 0.92f : 0.75f), withAlpha(p.textShadow, 0.6f));
  } else {
    const float iconSize = icon == Icon::None ? 0.0f : canvas_.capHeight(font) * 1.35f;
    const float gap = icon == Icon::None ? 0.0f : 10.0f;
    const float tw = canvas_.textWidth(font, label);
    const float x0 = r.cx() - (tw + iconSize + gap) * 0.5f;
    if (icon != Icon::None) {
      draw_icon(canvas_, icon, Vec2{x0 + iconSize * 0.5f + 1, r.cy() + 1.5f}, iconSize, withAlpha(p.textShadow, 0.5f));
      draw_icon(canvas_, icon, Vec2{x0 + iconSize * 0.5f, r.cy()}, iconSize, text);
    }
    canvas_.pushOffset(0, 0);
    canvas_.textShadow(font, x0 + iconSize + gap, r.cy() + canvas_.capHeight(font) * 0.5f, label, text, p.textShadow,
                       Align::Left, 1.8f);
    canvas_.pop();
  }
  canvas_.pop();
  const bool act = enabled && !blocked_ && activated_ == id;
  if (act) {
    flashId_ = id;
    flashTime_ = 0.12f;
  }
  return act;
}

void App::settingRow(int id, Rect r, Setting& s) {
  reg(id, r, s.toggle ? Kind::Toggle : Kind::Cycler);
  const float hv = hover(id);
  int delta = 0;
  // A click on the left arrow steps back; anywhere else on the row (or Enter / A) steps forward.
  const float leftArrow = r.x + r.w - 220;
  if (!blocked_ && activated_ == id) delta = !s.toggle && clickX_ >= 0 && clickX_ < leftArrow + 36 ? -1 : 1;
  if (!blocked_ && focus_ == id && cycle_) delta = cycle_;
  if (delta) {
    const int n = static_cast<int>(s.choices.size());
    if (s.toggle) s.index = s.index ? 0 : 1;
    else s.index = ((s.index + delta) % n + n) % n;
    s.dirty = true;
    sfx_play(s.toggle ? Sfx::Toggle : Sfx::Click);
    if (!std::strcmp(s.name, "sounds")) {
      sfx_enable(s.value() == "1");
      sfx_play(Sfx::Toggle);
    }
  }
  if (hv > 0.01f) {
    canvas_.roundRect(r, 12, rgb(0xFFFFFF, 0.20f * hv), rgb(0xFFFFFF, 0.12f * hv));
    canvas_.roundRectStroke(r, 12, 1.5f, rgb(0xFFFFFF, 0.55f * hv));
  }
  canvas_.textMid(kFontBody, r.x + 16, r.cy(), s.label, rgb(0xFFFFFF));
  if (s.toggle) {
    float& t = toggleAnim_[id];
    t = Approach(t, s.index ? 1.0f : 0.0f, 0.3f);
    const Rect track{r.x + r.w - 66, r.cy() - 13, 50, 26};
    canvas_.roundRect(track, 13, mix(rgb(0x5F7EA6), rgb(0x8EE85A), t), mix(rgb(0x47678F), rgb(0x34A126), t));
    canvas_.roundRectStroke(track, 13, 1.2f, rgb(0xFFFFFF, 0.5f));
    const Vec2 knob{track.x + 13 + t * 24, track.cy()};
    canvas_.softCircle(Vec2{knob.x, knob.y + 2}, 14, rgb(0x062457, 0.35f));
    canvas_.circleGradient(knob, 10.5f, rgb(0xFFFFFF), rgb(0xDCE8F5));
  } else {
    const float right = r.x + r.w - 14, left = leftArrow;
    const float mid = (left + right) * 0.5f;
    const Color arrow = rgb(0xFFFFFF, 0.55f + 0.45f * hv);
    canvas_.chevron(Vec2{left + 6, r.cy()}, 13, -1, 2.6f, arrow);
    canvas_.chevron(Vec2{right - 6, r.cy()}, 13, 1, 2.6f, arrow);
    const std::string v = canvas_.ellipsize(kFontHeading, s.choices[static_cast<size_t>(s.index)].label, right - left - 34);
    canvas_.textMid(kFontHeading, mid, r.cy(), v, s.index == s.defaultIndex ? rgb(0xFFFFFF) : rgb(0xFFF3A6), Align::Center);
  }
}

bool App::layoutCard(int id, Rect r, int split, const char* title, const char* help, bool selected) {
  reg(id, r, Kind::Button);
  const float hv = hover(id);
  float& sel = toggleAnim_[id];
  sel = Approach(sel, selected ? 1.0f : 0.0f, 0.3f);
  canvas_.pushZoom(r.cx(), r.cy(), 1.0f + 0.02f * hv);
  if (sel > 0.01f) canvas_.roundRectGlow(r, 18, 14, rgb(0xB6FF7A, 0.55f * sel));
  canvas_.roundRect(r, 18, mix(rgb(0xFFFFFF, 0.14f), rgb(0xFFFFFF, 0.26f), hv), mix(rgb(0xFFFFFF, 0.06f), rgb(0xFFFFFF, 0.14f), hv));
  canvas_.roundRectStroke(r, 18, 2.0f, mix(rgb(0xFFFFFF, 0.35f + 0.3f * hv), rgb(0x9CF061), sel));
  draw_layout_picture(canvas_, Rect{r.x + 10, r.y + 8, r.w - 20, 98}, split, std::max(hv, sel));
  canvas_.text(kFontHeading, r.x + 16, r.y + 128, title, rgb(0xFFFFFF));
  canvas_.textWrapped(kFontTiny, r.x + 16, r.y + 136, r.w - 32, help, rgb(0xDCEEFF), 1.05f, 2);
  if (sel > 0.01f) {
    const Vec2 b{r.x + r.w - 20, r.y + 20};
    canvas_.circleGradient(b, 12 * sel, rgb(0xA4EE62), rgb(0x34A126));
    canvas_.check(b, 11 * sel, 2.4f, rgb(0xFFFFFF));
  }
  canvas_.pop();
  return !blocked_ && activated_ == id;
}

void App::panel(Rect r, const std::string& title) {
  canvas_.roundRectGlow(Rect{r.x, r.y + 8, r.w, r.h}, 22, 22, rgb(0x062457, 0.30f));
  canvas_.roundRect(r, 22, rgb(0x2C7AD6, 0.93f), rgb(0x1A4A99, 0.95f));
  canvas_.roundRect(Rect{r.x + 3, r.y + 3, r.w - 6, std::min(56.0f, r.h * 0.4f)}, 19, rgb(0xFFFFFF, 0.16f), rgb(0xFFFFFF, 0.0f));
  canvas_.roundRectStroke(r, 22, 2.0f, rgb(0xA6D8FF, 0.75f));
  if (!title.empty()) canvas_.textShadow(kFontLabel, r.x + 22, r.y + 30, title, rgb(0xCFEBFF), rgb(0x0B2F6B, 0.5f));
}

void App::chip(float x, float y, const std::string& text, Color bg, Color fg, float* width) {
  const float w = canvas_.textWidth(kFontLabel, text) + 22;
  canvas_.roundRect(Rect{x, y, w, 26}, 13, bg, mix(bg, rgb(0x000000, bg.a), 0.12f));
  canvas_.roundRectStroke(Rect{x, y, w, 26}, 13, 1.0f, rgb(0xFFFFFF, 0.35f));
  canvas_.textMid(kFontLabel, x + 11, y + 13, text, fg);
  if (width) *width = w;
}

void App::setPage(Page p, int focus) {
  page_ = p;
  pageTime_ = 0;
  focus_ = focus;
  heldDx_ = heldDy_ = 0;
}

// ---- pages ----------------------------------------------------------------------------------------

void App::drawHeader(const std::string& title, const std::string& subtitle) {
  draw_plumbob(canvas_, 86, 60 + std::sin(time_ * 1.6f) * 3, 62, time_ * 1.2f, 0.8f, false);
  canvas_.textShadow(kFontTitle, 130, 70, title, rgb(0xFFFFFF), rgb(0x0B2F6B, 0.45f), Align::Left, 2.5f);
  canvas_.textShadow(kFontBody, 132, 97, subtitle, rgb(0xFFFFFF), rgb(0x0B2F6B, 0.55f), Align::Left, 1.5f);
}

void App::drawHome() {
  // Title.
  const float bob = std::sin(time_ * 1.6f) * 4.0f;
  draw_plumbob(canvas_, kW * 0.5f, 72 + bob, 104, time_ * 1.1f, 1.0f, false);
  draw_wordmark(canvas_, kW * 0.5f, 232);
  canvas_.textShadow(kFontHeading, kW * 0.5f, 272, "The Sims 2 \xC2\xB7 GameCube \xC2\xB7 now on your PC", rgb(0xFFFFFF),
                     rgb(0x0B2F6B, 0.7f), Align::Center, 2.0f);

  // The disc card.
  const Rect card{60, 298, 520, 318};
  panel(card, "YOUR DISC");
  const Color hole = rgb(0x235CB2);
  if (!haveDisc_ || disc_.status == DiscStatus::Missing) {
    const bool missing = haveDisc_;
    draw_disc(canvas_, card.x + 110, card.y + 140, 62, time_, false, hole);
    const float tx = card.x + 200;
    canvas_.text(kFontTitle, tx, card.y + 106, missing ? "Disc not found" : "Insert your disc", rgb(0xFFFFFF));
    canvas_.textWrapped(kFontBody, tx, card.y + 120, card.w - 222,
                        missing ? "SIMMY can't find the disc image it used last time:\n" + FileName(disc_.path)
                                : "Choose your copy of The Sims 2 for GameCube, as an .iso or .gcm disc image. SIMMY remembers it for next time.",
                        rgb(0xE4F2FF), 1.08f, 4);
    if (button(kIdChooseDisc, Rect{tx, card.y + 222, 236, 52}, "Choose disc\xE2\x80\xA6", Style::Blue, Icon::Disc)) {
      sfx_play(Sfx::Click);
      openDiscDialog();
    }
    canvas_.text(kFontTiny, tx + 4, card.y + 296, "or drop the file on this window", rgb(0xCFE6FF, 0.85f));
  } else if (!disc_.ok()) {
    draw_disc(canvas_, card.x + 110, card.y + 136, 62, time_ * 0.4f, true, hole);
    canvas_.circleGradient(Vec2{card.x + 152, card.y + 92}, 19, rgb(0xFFB25A), rgb(0xE8661C));
    canvas_.ring(Vec2{card.x + 152, card.y + 92}, 19, 2, rgb(0xFFFFFF, 0.9f));
    canvas_.textMid(kFontButton, card.x + 152, card.y + 92, "!", rgb(0xFFFFFF), Align::Center);
    const float tx = card.x + 200;
    const char* title = disc_.status == DiscStatus::Compressed  ? "Compressed image"
                        : disc_.status == DiscStatus::NKit      ? "NKit image"
                        : disc_.status == DiscStatus::Wii       ? "That's a Wii disc"
                        : disc_.status == DiscStatus::WrongGame ? "Wrong game"
                        : disc_.status == DiscStatus::Damaged   ? "Incomplete image"
                                                                : "Not a GameCube disc";
    canvas_.text(kFontButtonBig, tx, card.y + 94, canvas_.ellipsize(kFontButtonBig, title, card.w - 222), rgb(0xFFFFFF));
    canvas_.textWrapped(kFontSmall, tx, card.y + 108, card.w - 222, disc_.message, rgb(0xFFE9C9), 1.08f, 5);
    if (button(kIdChooseDisc, Rect{tx, card.y + 222, 256, 52}, "Choose another\xE2\x80\xA6", Style::Orange, Icon::Disc)) {
      sfx_play(Sfx::Click);
      openDiscDialog();
    }
    canvas_.text(kFontTiny, tx + 4, card.y + 296, canvas_.ellipsize(kFontTiny, FileName(disc_.path), card.w - 230), rgb(0xCFE6FF, 0.85f));
  } else {
    // The disc's own banner, as the GameCube's menu showed it.
    const Rect b{card.x + 26, card.y + 50, 288, 96};
    canvas_.roundRectGlow(Rect{b.x - 6, b.y - 2, b.w + 12, b.h + 12}, 10, 12, rgb(0x061F48, 0.4f));
    canvas_.roundRect(Rect{b.x - 6, b.y - 6, b.w + 12, b.h + 12}, 10, rgb(0xFFFFFF), rgb(0xDCE8F5));
    if (banner_) canvas_.image(banner_, b);
    else canvas_.rect(b, rgb(0x4B8FE0), rgb(0x23559F));
    canvas_.rect(Rect{b.x, b.y, b.w, b.h * 0.45f}, rgb(0xFFFFFF, 0.16f), rgb(0xFFFFFF, 0.0f));  // a little shine
    draw_disc(canvas_, card.x + 420, card.y + 100, 56, time_ * 0.9f, true, hole);
    const std::string title = disc_.displayTitle().empty() ? std::string("The Sims 2") : disc_.displayTitle();
    canvas_.text(kFontTitle, card.x + 24, card.y + 196, canvas_.ellipsize(kFontTitle, title, card.w - 48), rgb(0xFFFFFF));
    std::string line = disc_.displayMaker();
    if (!disc_.description.empty()) line += (line.empty() ? "" : "  \xC2\xB7  ") + disc_.description;
    canvas_.text(kFontSmall, card.x + 25, card.y + 222, canvas_.ellipsize(kFontSmall, line, card.w - 50), rgb(0xD7ECFF));
    float x = card.x + 24, w = 0;
    chip(x, card.y + 238, disc_.gameId, rgb(0x0E3A7A, 0.6f), rgb(0xFFFFFF), &w);
    x += w + 8;
    const std::string video = disc_.videoName();
    chip(x, card.y + 238, disc_.regionName() + (video.empty() ? "" : " \xC2\xB7 " + video), rgb(0x0E3A7A, 0.6f),
         rgb(0xFFFFFF), &w);
    x += w + 8;
    chip(x, card.y + 238, "Disc " + std::to_string(disc_.discNumber + 1) + " \xC2\xB7 Rev " + std::to_string(disc_.version),
         rgb(0x0E3A7A, 0.6f), rgb(0xFFFFFF), &w);
    canvas_.text(kFontTiny, card.x + 26, card.y + 296, canvas_.ellipsize(kFontTiny, FileName(disc_.path), 300), rgb(0xCFE6FF, 0.8f));
    if (button(kIdChooseDisc, Rect{card.x + card.w - 176, card.y + 268, 152, 38}, "Change disc", Style::Ghost, Icon::None, true,
               kFontHeading)) {
      sfx_play(Sfx::Click);
      openDiscDialog();
    }
  }

  // The big buttons.
  const bool ok = disc_.ok();
  if (button(kIdPlay, Rect{620, 298, 420, 100}, "Play", Style::Green, Icon::Play, ok, kFontButtonBig,
             "One player \xC2\xB7 start the game"))
    launch(false);
  if (button(kIdCoop, Rect{620, 414, 420, 82}, "Co-op", Style::Blue, Icon::People, ok, kFontButtonBig,
             pads_.size() >= 2 ? "Two players \xC2\xB7 2 controllers ready" : "Two players \xC2\xB7 split screen or two windows")) {
    sfx_play(Sfx::Click);
    setPage(Page::Coop, kIdCoopStart);
  }
  if (button(kIdOptions, Rect{620, 512, 204, 58}, "Options", Style::Blue, Icon::Gear)) {
    sfx_play(Sfx::Click);
    setPage(Page::Options, kIdSetting0);
  }
  if (button(kIdQuit, Rect{836, 512, 204, 58}, "Quit", Style::Ghost, Icon::Power)) quit_ = true;
  if (!ok) {
    const std::string hint = haveDisc_ ? "Choose a working disc to play" : "Choose your disc first, then press Play";
    const float w = canvas_.textWidth(kFontSmall, hint) + 36;
    canvas_.roundRect(Rect{830 - w * 0.5f, 582, w, 30}, 15, rgb(0x0E3A7A, 0.55f), rgb(0x0E3A7A, 0.65f));
    canvas_.textMid(kFontSmall, 830, 597, hint, rgb(0xFFFFFF), Align::Center);
  }
}

void App::drawCoop() {
  drawHeader("Two-player co-op", "Two Sims, one household. Choose how the screen is shared.");
  const Rect left{60, 120, 610, 446};
  panel(left, "SCREEN LAYOUT");
  struct Layout {
    const char* value;
    int split;
    const char* title;
    const char* help;
  };
  static const Layout kLayouts[] = {
      {"windows", 3, "Two windows", "Each player gets a window \xE2\x80\x93 put one on a second monitor or TV."},
      {"wide", 2, "One wide window", "Your screen's shape cut down the middle."},
      {"4x3", 1, "Two 4:3 halves", "Two classic 4:3 pictures side by side."},
      {"gc", 0, "GameCube original", "One 4:3 picture split in two, like on the console."},
  };
  for (int i = 0; i < 4; ++i) {
    const Rect r{left.x + 22 + static_cast<float>(i % 2) * 290, left.y + 44 + static_cast<float>(i / 2) * 196, 276, 184};
    const Layout& l = kLayouts[i];
    if (layoutCard(kIdLayout0 + l.split, r, l.split, l.title, l.help, coopDisplay_ == l.value) && coopDisplay_ != l.value) {
      coopDisplay_ = l.value;
      ini_.setLauncher("coop_display", coopDisplay_);
      std::string message;
      ini_.save(message);
      sfx_play(Sfx::Toggle);
    }
  }

  const Rect pads{690, 120, 350, 262};
  panel(pads, "CONTROLLERS");
  for (int i = 0; i < 2; ++i) {
    const float y = pads.y + 46 + static_cast<float>(i) * 92;
    const bool on = static_cast<int>(pads_.size()) > i;
    const Color accent = i == 0 ? rgb(0x3E95F5) : rgb(0xF5A23E);
    canvas_.roundRect(Rect{pads.x + 16, y, pads.w - 32, 80}, 16, rgb(0xFFFFFF, on ? 0.14f : 0.06f), rgb(0xFFFFFF, on ? 0.08f : 0.03f));
    const float bounce = on ? std::fabs(std::sin(time_ * 2.2f + static_cast<float>(i))) * 2.0f : 0.0f;
    draw_controller(canvas_, pads.x + 76, y + 40 - bounce, 82, on, accent);
    canvas_.circleGradient(Vec2{pads.x + 36, y + 18}, 14, mix(accent, rgb(0xFFFFFF), 0.3f), accent);
    canvas_.textMid(kFontLabel, pads.x + 36, y + 18, i == 0 ? "P1" : "P2", rgb(0xFFFFFF), Align::Center);
    const float tx = pads.x + 134;
    if (on) {
      const char* name = SDL_GetGamepadName(pads_[static_cast<size_t>(i)]);
      canvas_.text(kFontHeading, tx, y + 36, canvas_.ellipsize(kFontHeading, name ? name : "Controller", pads.w - 160), rgb(0xFFFFFF));
      canvas_.circle(Vec2{tx + 5, y + 55}, 5, rgb(0x8EF25A));
      canvas_.text(kFontSmall, tx + 16, y + 60, "Connected", rgb(0xC9F7B0));
    } else {
      canvas_.text(kFontHeading, tx, y + 36, i == 0 ? "No controller" : "Waiting for player 2", rgb(0xFFFFFF, 0.8f));
      canvas_.text(kFontSmall, tx, y + 60, i == 0 ? "Keyboard and mouse work too" : "Plug in a second controller",
                   rgb(0xDCEEFF, 0.75f));
    }
  }
  if (pads_.size() < 2)
    canvas_.textWrapped(kFontTiny, pads.x + 22, pads.y + pads.h - 26, pads.w - 44,
                        "You can start anyway \xE2\x80\x93 player 2 can join later.", rgb(0xFFE7A8), 1.0f, 1);

  const Rect quick{690, 394, 350, 172};
  panel(quick, "QUICK START");
  Setting* q = setting("quick_coop");
  settingRow(kIdQuickCoop, Rect{quick.x + 12, quick.y + 42, quick.w - 24, 44}, *q);
  canvas_.textWrapped(kFontTiny, quick.x + 26, quick.y + 96, quick.w - 52,
                      q->value() == "1" ? "Skips the intro, finishes Create-a-Sim after a few seconds and puts both players straight into the house."
                                        : "The full menus: pick New Game and Two Player; player 2 presses A when asked.",
                      rgb(0xDCEEFF), 1.05f, 3);
  if (q->dirty) saveSettings(true);

  if (button(kIdCoopBack, Rect{60, 580, 170, 52}, "Back", Style::Ghost, Icon::Back)) back();
  if (button(kIdCoopStart, Rect{760, 576, 280, 60}, "Start co-op", Style::Green, Icon::Play, disc_.ok(), kFontButton))
    launch(true);
}

void App::drawOptions() {
  drawHeader("Options", "Saved to simmy.ini next to the game. Press F1 in the game for even more.");
  const Rect display{60, 120, 490, 302}, graphics{570, 120, 470, 260}, game{570, 390, 470, 240};
  panel(display, "DISPLAY");
  panel(graphics, "GRAPHICS");
  panel(game, "CONTROLS, GAME & LAUNCHER");
  const Rect boxes[3] = {display, graphics, game};
  int row[3] = {0, 0, 0};
  const Setting* focused = nullptr;
  for (size_t i = 0; i < settings_.size(); ++i) {
    Setting& s = settings_[i];
    const Rect& b = boxes[s.group];
    const float pitch = s.group == 2 ? 39.0f : 42.0f;  // (the controls panel is the tallest: rows a little closer)
    const Rect r{b.x + 12, b.y + (s.group == 2 ? 37.0f : 40.0f) + static_cast<float>(row[s.group]++) * pitch, b.w - 24, pitch - 2.0f};
    settingRow(kIdSetting0 + static_cast<int>(i), r, s);
    if (focus_ == kIdSetting0 + static_cast<int>(i)) focused = &s;
  }
  // The focused setting's explanation.
  const Rect help{60, 436, 490, 132};
  canvas_.roundRect(help, 18, rgb(0x123F84, 0.82f), rgb(0x0E3370, 0.86f));
  canvas_.roundRectStroke(help, 18, 1.2f, rgb(0xFFFFFF, 0.3f));
  if (focused) {
    canvas_.text(kFontHeading, help.x + 20, help.y + 32, focused->label, rgb(0xFFFFFF));
    canvas_.textWrapped(kFontSmall, help.x + 20, help.y + 42, help.w - 40, focused->help, rgb(0xDCEEFF), 1.1f, 3);
  } else {
    canvas_.textWrapped(kFontSmall, help.x + 20, help.y + 20, help.w - 40,
                        "Pick a setting to see what it does. Changes are saved when you go back, and the game uses them the next time it starts.",
                        rgb(0xDCEEFF), 1.1f, 4);
  }
  if (button(kIdOptionsBack, Rect{60, 580, 170, 52}, "Back", Style::Ghost, Icon::Back)) back();
  if (button(kIdOptionsReset, Rect{246, 580, 220, 52}, "Reset to defaults", Style::Ghost, Icon::None, true, kFontHeading)) {
    for (Setting& s : settings_)
      if (!s.launcherKey && s.index != s.defaultIndex) s.index = s.defaultIndex, s.dirty = true;
    sfx_play(Sfx::Back);
    toast_ = Toast{"Graphics settings back to the GameCube defaults", 0};
  }
}

void App::drawPlaying() {
  const float spin = time_ * 2.4f;
  draw_plumbob(canvas_, kW * 0.5f, 210 + std::sin(time_ * 2.0f) * 6, 170, spin, 1.0f, true);
  canvas_.ellipseSoft(Vec2{kW * 0.5f, 470}, 460, 120, rgb(0x0B3A82, 0.35f));
  canvas_.textShadow(kFontTitle, kW * 0.5f, 432, "Sul sul! Have fun.", rgb(0xFFFFFF), rgb(0x0B2F6B, 0.45f), Align::Center, 2.5f);
  canvas_.textShadow(kFontBody, kW * 0.5f, 466, "The Sims 2 is running in its own window. The launcher comes back when you quit.",
                     rgb(0xFFFFFF), rgb(0x0B2F6B, 0.6f), Align::Center, 1.5f);
  float w = 0;
  const float cw = canvas_.textWidth(kFontLabel, runningMode_) + 22;
  chip(kW * 0.5f - cw * 0.5f, 486, runningMode_, rgb(0x0E3A7A, 0.55f), rgb(0xFFFFFF), &w);
  static const char* const kTips[] = {
      "Reticulating splines\xE2\x80\xA6",       "Polishing the plumbob\xE2\x80\xA6",    "Fluffing the sofa cushions\xE2\x80\xA6",
      "Teaching the toddlers Simlish\xE2\x80\xA6", "Watering the neighbourhood\xE2\x80\xA6", "Tuning the stereo to Simlish pop\xE2\x80\xA6"};
  const int tip = static_cast<int>(time_ / 3.2f) % 6;
  const float phase = std::fmod(time_, 3.2f);
  const float a = std::min(1.0f, std::min(phase / 0.4f, (3.2f - phase) / 0.4f));
  canvas_.text(kFontSmall, kW * 0.5f, 548, kTips[tip], rgb(0xFFFFFF, 0.85f * a), Align::Center);
  if (button(kIdForceQuit, Rect{kW * 0.5f - 85, 588, 170, 46}, "Force quit", Style::Ghost, Icon::Stop, game_.running(),
             kFontHeading)) {
    forceQuit_ = true;
    game_.kill();
  }
}

void App::drawError() {
  if (!error_.open) return;
  canvas_.rect(Rect{0, 0, kW, kH}, rgb(0x04122E, 0.55f), rgb(0x04122E, 0.65f));
  const Rect r{kW * 0.5f - 330, 130, 660, 420};
  canvas_.roundRectGlow(Rect{r.x, r.y + 10, r.w, r.h}, 26, 30, rgb(0x000814, 0.45f));
  canvas_.roundRect(r, 26, rgb(0x2C7AD6), rgb(0x18418A));
  canvas_.roundRect(Rect{r.x + 3, r.y + 3, r.w - 6, 60}, 23, rgb(0xFFFFFF, 0.16f), rgb(0xFFFFFF, 0.0f));
  canvas_.roundRectStroke(r, 26, 2.0f, rgb(0xFFC27A, 0.9f));
  const Vec2 badge{r.x + 62, r.y + 66};
  canvas_.softCircle(badge, 52, rgb(0xFFB25A, 0.45f));
  canvas_.circleGradient(badge, 30, rgb(0xFFC170), rgb(0xE85E1C));
  canvas_.ring(badge, 30, 2.5f, rgb(0xFFFFFF, 0.9f));
  canvas_.textMid(kFontTitle, badge.x, badge.y, "!", rgb(0xFFFFFF), Align::Center);
  canvas_.text(kFontTitle, r.x + 112, r.y + 66, canvas_.ellipsize(kFontTitle, error_.title, r.w - 140), rgb(0xFFFFFF));
  const float bodyH = canvas_.textWrapped(kFontBody, r.x + 36, r.y + 110, r.w - 72, error_.body, rgb(0xE8F4FF), 1.1f, 5);
  if (!error_.excerpt.empty()) {
    const Rect box{r.x + 30, r.y + 122 + bodyH, r.w - 60, r.h - 214 - bodyH};
    if (box.h > 30) {
      canvas_.roundRect(box, 12, rgb(0x061A3D, 0.6f), rgb(0x061A3D, 0.7f));
      float y = box.y + 22;
      size_t start = 0;
      while (start < error_.excerpt.size() && y < box.y + box.h - 6) {
        size_t end = error_.excerpt.find('\n', start);
        if (end == std::string::npos) end = error_.excerpt.size();
        canvas_.text(kFontTiny, box.x + 14, y, canvas_.ellipsize(kFontTiny, error_.excerpt.substr(start, end - start), box.w - 28),
                     rgb(0xBFDCFF));
        y += 17;
        start = end + 1;
      }
    }
  }
  const bool saved = blocked_;
  blocked_ = false;
  if (error_.offerFolder && button(kIdErrorFolder, Rect{r.x + 30, r.y + r.h - 78, 250, 52}, "Open log folder", Style::Blue, Icon::Folder)) {
    sfx_play(Sfx::Click);
    open_folder(baseDir_);
  }
  if (button(kIdErrorOk, Rect{r.x + r.w - 190, r.y + r.h - 78, 160, 52}, "OK", Style::Green, Icon::None)) {
    error_.open = false;
    sfx_play(Sfx::Back);
    focus_ = page_ == Page::Home ? kIdPlay : focus_;
  }
  blocked_ = saved;
}

void App::drawToast() {
  if (toast_.text.empty()) return;
  const float t = toast_.time;
  if (t > 3.2f) {
    toast_.text.clear();
    return;
  }
  const float a = std::min(1.0f, std::min(t / 0.2f, (3.2f - t) / 0.4f));
  const float w = canvas_.textWidth(kFontSmall, toast_.text) + 48;
  const Rect r{kW * 0.5f - w * 0.5f, 18 - (1 - Ease(t / 0.25f)) * 20, w, 38};
  const float saved = canvas_.alpha;
  canvas_.alpha *= a;
  canvas_.roundRectGlow(Rect{r.x, r.y + 4, r.w, r.h}, 19, 12, rgb(0x062457, 0.3f));
  canvas_.roundRect(r, 19, rgb(0xFFFFFF, 0.96f), rgb(0xE4F0FC, 0.96f));
  canvas_.circle(Vec2{r.x + 20, r.cy()}, 6, rgb(0x46C42F));
  canvas_.textMid(kFontSmall, r.x + 34, r.cy(), toast_.text, rgb(0x163F7A));
  canvas_.alpha = saved;
}

void App::drawHints() {
  // Bottom bar: controllers on the left, controls on the right.
  const float y = 662;
  if (page_ == Page::Home) {
    const int n = static_cast<int>(pads_.size());
    for (int i = 0; i < std::min(n, 4); ++i) draw_controller(canvas_, 86 + static_cast<float>(i) * 34, y - 4, 28, true, i ? rgb(0xF5A23E) : rgb(0x3E95F5));
    const std::string text = n == 0 ? "No controllers \xE2\x80\x93 keyboard and mouse work too"
                             : n == 1 ? "1 controller connected"
                                      : std::to_string(n) + " controllers connected";
    canvas_.textShadow(kFontSmall, n ? 86 + static_cast<float>(std::min(n, 4)) * 34 - 4 : 62, y, text, rgb(0xFFFFFF, 0.92f),
                       rgb(0x0B2F6B, 0.45f), Align::Left);
  }
  if (page_ == Page::Playing) return;
  const bool pad = !pads_.empty();
  struct Hint {
    const char* key;
    const char* what;
    Color color;
  };
  const Hint hints[2] = {{pad ? "A" : "Enter", "Select", rgb(0x3FB43A)}, {pad ? "B" : "Esc", "Back", rgb(0xD84A4A)}};
  float x = kW - 62;
  for (int i = page_ == Page::Home ? 0 : 1; i >= 0; --i) {
    const Hint& h = hints[i];
    const float tw = canvas_.textWidth(kFontSmall, h.what);
    x -= tw;
    canvas_.textShadow(kFontSmall, x, y, h.what, rgb(0xFFFFFF, 0.92f), rgb(0x0B2F6B, 0.45f));
    const float kw = std::max(24.0f, canvas_.textWidth(kFontLabel, h.key) + 14);
    x -= kw + 8;
    const Rect k{x, y - 17, kw, 22};
    canvas_.roundRect(k, pad ? 11 : 6, pad ? h.color : rgb(0xFFFFFF, 0.9f), pad ? mix(h.color, rgb(0x000000), 0.2f) : rgb(0xD6E4F2, 0.9f));
    canvas_.textMid(kFontLabel, k.cx(), k.cy(), h.key, pad ? rgb(0xFFFFFF) : rgb(0x163F7A), Align::Center);
    x -= 22;
  }
}

void App::frame(float dt) {
  time_ += dt;
  pageTime_ += dt;
  toast_.time += dt;
  flashTime_ -= dt;
  activated_ = pendingActivate_;
  pendingActivate_ = 0;
  cycle_ = pendingCycle_;
  pendingCycle_ = 0;
  if (pendingBack_) {
    pendingBack_ = false;
    back();
  }
  prevItems_.swap(items_);
  items_.clear();

  if (surface_) {
    SDL_SetRenderDrawColor(renderer_, 30, 100, 200, 255);
    SDL_RenderClear(renderer_);
  }
  draw_sky(canvas_, time_ + 40.0f, kW, kH);
  const float appear = Ease(pageTime_ / 0.3f);
  canvas_.alpha = appear;
  canvas_.pushOffset(0, (1 - appear) * 14);
  blocked_ = error_.open;
  switch (page_) {
  case Page::Home: drawHome(); break;
  case Page::Coop: drawCoop(); break;
  case Page::Options: drawOptions(); break;
  case Page::Playing: drawPlaying(); break;
  }
  canvas_.pop();
  canvas_.alpha = 1;
  blocked_ = false;
  drawHints();
  drawError();
  drawToast();
  canvas_.flush();
  mouseMoved_ = false;
  // The focused widget must exist; otherwise take the page's first.
  bool found = false;
  for (const Item& it : items_) found = found || it.id == focus_;
  if (!found && !items_.empty()) focus_ = items_.front().id;
}

bool App::screenshotNow(const std::string& path) {
  SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
  if (!shot) {
    std::fprintf(stderr, "[launcher] screenshot failed: %s\n", SDL_GetError());
    return false;
  }
  const bool ok = SDL_SaveBMP(shot, path.c_str());
  std::fprintf(stderr, "[launcher] %s %dx%d screenshot %s\n", ok ? "saved" : "could not save", shot->w, shot->h, path.c_str());
  SDL_DestroySurface(shot);
  return ok;
}

int App::run() {
  if (!createWindow()) return 1;
  g_dialogEvent = SDL_RegisterEvents(1);
  if (const char* base = SDL_GetBasePath()) baseDir_ = base;
  const char* iniEnv = std::getenv("SIMMY_INI");  // the game reads the same variable
  iniPath_ = iniEnv && *iniEnv ? iniEnv : baseDir_ + "simmy.ini";
  const bool test = !opt_.screenshot.empty();
  if (!test) sfx_init();
  loadIni();

  if (opt_.fakePads >= 0) {
    SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    static const char* const kNames[] = {"Xbox Wireless Controller", "GameCube Controller Adapter", "DualSense", "Pro Controller"};
    for (int i = 0; i < opt_.fakePads && i < 4; ++i) {
      SDL_VirtualJoystickDesc desc;
      SDL_INIT_INTERFACE(&desc);
      desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
      desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
      desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
      desc.name = kNames[i];
      SDL_AttachVirtualJoystick(&desc);
    }
  }
  openPads();

  if (!opt_.choose.empty()) setDisc(opt_.choose, true, true);
  for (const std::string& kv : opt_.sets) {
    const size_t eq = kv.find('=');
    Setting* s = eq == std::string::npos ? nullptr : setting(kv.substr(0, eq).c_str());
    if (!s) continue;
    for (size_t i = 0; i < s->choices.size(); ++i)
      if (s->choices[i].value == kv.substr(eq + 1)) s->index = static_cast<int>(i), s->dirty = true;
  }
  if (!opt_.choose.empty()) {
    // already set above
  } else if (!opt_.testIso.empty()) {
    setDisc(opt_.testIso, false, false);
  } else if (!ini_.get("iso").empty()) {
    setDisc(ini_.get("iso"), false, false);
  }
  focus_ = disc_.ok() ? kIdPlay : kIdChooseDisc;

  if (opt_.page == "coop") setPage(Page::Coop, kIdCoopStart);
  else if (opt_.page == "options") setPage(Page::Options, kIdSetting0);
  else if (opt_.page == "playing") {
    runningMode_ = std::string("Co-op \xC2\xB7 Two windows");
    setPage(Page::Playing, kIdForceQuit);
  } else if (opt_.page == "error") {
    showError("Uh-oh! Something went wrong",
              "The game stopped unexpectedly: the game crashed (access violation). Exit code 0xC0000005. "
              "The logs (game_log.txt, game_out.txt, simmy_crash.txt) are in the launcher's folder; they help a lot when reporting the problem.",
              "SIMMY crash: exception 0xC0000005 (access violation) at 0x00007FF6A1B2C3D4\nsimmy.exe+0x12C3D4  EAnimController::Update\nsimmy.exe+0x0A1F00  ESimsCam::Tick\nsimmy.exe+0x004410  ENgcEngine::Idle\n",
              true);
  }
  if (opt_.focus) focus_ = opt_.focus;
  if (opt_.mouseX >= 0) mx_ = opt_.mouseX, my_ = opt_.mouseY, mouseMoved_ = true;
  if (!opt_.autoLaunch.empty()) autoLaunchFrame_ = 3;

  Uint64 last = SDL_GetTicksNS();
  int frames = 0;
  while (!quit_) {
    SDL_Event e;
    if (game_.running() && (minimized_ || !window_)) {
      // Asleep while the game has the stage.
      if (SDL_WaitEventTimeout(&e, 100)) handleEvent(e);
    }
    while (SDL_PollEvent(&e)) handleEvent(e);
    pollGame();
    const Uint64 now = SDL_GetTicksNS();
    float dt = static_cast<float>(now - last) / 1e9f;
    last = now;
    if (test || !opt_.autoLaunch.empty()) dt = 1.0f / 60.0f;
    dt = std::min(dt, 0.1f);
    heldRepeat(dt);
    if (game_.running() && minimized_ && window_) continue;
    frame(dt);
    ++frames;
    if (autoLaunchFrame_ > 0 && --autoLaunchFrame_ == 0) launch(opt_.autoLaunch == "coop");
    if (afterGameFrames_ >= 0 && !opt_.autoLaunch.empty()) ++afterGameFrames_;
    const bool autoDone = !opt_.autoLaunch.empty() && afterGameFrames_ >= 40;
    if ((test && opt_.autoLaunch.empty() && frames >= opt_.frames) || (autoDone && test)) {
      screenshotNow(opt_.screenshot);
      break;
    }
    if (autoDone) break;
    SDL_RenderPresent(renderer_);
    if (!window_ || test) continue;
    // Software renderers have no vsync: about 60 frames a second.
    const Uint64 spent = SDL_GetTicksNS() - now;
    if (spent < 15000000) SDL_DelayNS(15000000 - spent);
  }
  saveSettings(true);
  for (SDL_Gamepad* p : pads_) SDL_CloseGamepad(p);
  pads_.clear();
  if (banner_) SDL_DestroyTexture(banner_);
  sfx_shutdown();
  SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);
  if (surface_) SDL_DestroySurface(surface_);
  SDL_Quit();
  return 0;
}

// --write-icon <dir>: the plumbob at the usual icon sizes (to make res/simmy_launcher.ico).
int WriteIcons(const std::string& dir) {
  for (int size : {16, 24, 32, 48, 64, 128, 256}) {
    SDL_Surface* s = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer* r = s ? SDL_CreateSoftwareRenderer(s) : nullptr;
    if (!r) return 1;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderClear(r);
    Canvas c;
    c.init(r, 1.0f, false);
    const float f = static_cast<float>(size);
    draw_plumbob(c, f * 0.5f, f * 0.42f, f * 0.80f, 0.35f, 0.0f, false);
    c.flush();
    SDL_RenderPresent(r);
    const std::string path = dir + "/icon_" + std::to_string(size) + ".bmp";
    SDL_SaveBMP(s, path.c_str());
    SDL_DestroyRenderer(r);
    SDL_DestroySurface(s);
  }
  return 0;
}

}  // namespace
}  // namespace launcher

int main(int argc, char** argv) {
  launcher::Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--screenshot") o.screenshot = next();
    else if (a == "--page") o.page = next();
    else if (a == "--frames") o.frames = std::atoi(next().c_str());
    else if (a == "--scale") o.scale = static_cast<float>(std::atof(next().c_str()));
    else if (a == "--fake-pads") o.fakePads = std::atoi(next().c_str());
    else if (a == "--focus") o.focus = std::atoi(next().c_str());
    else if (a == "--mouse") {
      const std::string v = next();
      o.mouseX = static_cast<float>(std::atof(v.c_str()));
      const size_t comma = v.find(',');
      o.mouseY = comma == std::string::npos ? 0.0f : static_cast<float>(std::atof(v.c_str() + comma + 1));
    } else if (a == "--test-iso" || a == "--iso") o.testIso = next();
    else if (a == "--game-exe") o.gameExe = next();
    else if (a == "--auto-launch") o.autoLaunch = next();
    else if (a == "--write-icon") o.writeIcon = next();
    else if (a == "--choose") o.choose = next();
    else if (a == "--set") o.sets.push_back(next());
  }
  if (!o.writeIcon.empty()) return launcher::WriteIcons(o.writeIcon);
  launcher::App app(o);
  return app.run();
}
