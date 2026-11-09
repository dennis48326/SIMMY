// PC only: keyboard and mouse for player 1 (no GameCube counterpart). See README "Keyboard and mouse".
//
// The keyboard is a GameCube pad on port 1: each game frame (ENgcControllerManager::Update, right after
// PADRead) the held keys become a PADStatus (main stick, buttons, triggers, d-pad, C stick) that is
// merged into port 1's (p1_input=auto: with the first controller's; keyboard_mouse: alone, the first
// controller goes to port 2; controller: no keyboard or mouse). aurora's own keyboard-to-pad bindings
// are cleared at startup (pc_main.cpp), so a key is never read twice.
//
// The mouse adds pad presses (left button = A, right click = B, the wheel = d-pad up/down in menus) and
// what a pad cannot give, in the frame's KbmTick (platform.hpp): the floor point under the mouse for the
// classic (cursor) control, camera rotation and zoom (drag, wheel) and camera panning (keys, screen
// edges). The game reads it where it reads the pads (siminteractor.cpp, directinteractor.cpp,
// camera.cpp); pc_det.cpp records and replays it with the pads, so SIMMY_DET replays match.
//
// The floor point: ELiveMode's draw notes each player's 3D view as drawn (kbm_note_view: world to clip
// matrix, eye, forward, viewport in the 640x480 logical frame). The mouse position in window 1 goes to
// the logical frame the way aurora presents it (the frame fitted into the window with the game's
// aspect, or stretched over it for widescreen / split views; split=windows: window 1 shows the left
// half of the frame), then into the player's viewport (the split views' halves), to normalised device
// coordinates, through the inverse matrix to a ray from the eye, and onto the cursor's plane (z = 0).
// So it follows aspect=, hud=, resolution= and split= as drawn.
//
// Test switch SIMMY_KBM_TEST="<frame>:<action>;..." (frames as --screenshot counts them, SIMMY_FRAMES_FROM_HOUSE
// applies): key:<name> / keyup:<name> / tap:<name>[/frames], move:<x>,<y> (window pixels), glide:<x>,<y>/<frames>,
// at:<object id or gGUID> (moves the mouse onto that object as drawn), ldown, lup, lclick, rdown, rup, rclick,
// mdown, mup, wheel:<notches>, drag:<dx>,<dy>/<frames> (relative motion), mouseout. Offscreen screenshots
// then show the mouse as a small cross. SIMMY_KBM_TRACE=1 logs the input made each frame it changes.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"
#include "pc_kbm.hpp"
#include "pc_overlay.hpp"
#include "pc_settings.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <aurora/aurora.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace simmy {

namespace {

// ---- bindings -----------------------------------------------------------------------------------

enum Action {
  kStickUp, kStickDown, kStickLeft, kStickRight, kWalk,
  kA, kB, kX, kY, kZ, kStart, kL, kR,
  kDUp, kDDown, kDLeft, kDRight,
  kCUp, kCDown, kCLeft, kCRight,
  kPanUp, kPanDown, kPanLeft, kPanRight,
  kActionCount
};

struct ActionDef {
  const char* name;      // [keys] spelling
  const char* defaults;  // default keys
  const char* label;     // the F1 overlay's Controls page
};
const ActionDef kActions[kActionCount] = {
    {"stick_up", "w", "Walk / move: up"},
    {"stick_down", "s", "Walk / move: down"},
    {"stick_left", "a", "Walk / move: left"},
    {"stick_right", "d", "Walk / move: right"},
    {"walk", "lshift, rshift", "Walk instead of run (half stick)"},
    {"a", "space, return, kp_enter", "A (choose, interact)"},
    {"b", "backspace", "B (back, cancel)"},
    {"x", "f", "X"},
    {"y", "tab", "Y (next sim)"},
    {"z", "c", "Z (classic cursor control)"},
    {"start", "escape", "Start (pause menu)"},
    {"l", "q", "L"},
    {"r", "e", "R"},
    {"dpad_up", "up", "D-pad up"},
    {"dpad_down", "down", "D-pad down"},
    {"dpad_left", "left", "D-pad left"},
    {"dpad_right", "right", "D-pad right"},
    {"cstick_up", "i", "C stick up (zoom in)"},
    {"cstick_down", "k", "C stick down (zoom out)"},
    {"cstick_left", "j", "C stick left (turn camera)"},
    {"cstick_right", "l", "C stick right (turn camera)"},
    {"pan_up", "kp_8", "Pan camera (mouse cursor control): up"},
    {"pan_down", "kp_2", "Pan camera: down"},
    {"pan_left", "kp_4", "Pan camera: left"},
    {"pan_right", "kp_6", "Pan camera: right"},
};

std::vector<SDL_Scancode> g_bind[kActionCount];
bool g_bindingsLoaded = false;

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
std::string Squash(const std::string& s) {  // lower case, no spaces, '_' or '-'
  std::string out;
  for (char c : Lower(s))
    if (c != ' ' && c != '_' && c != '-') out += c;
  return out;
}

// One key name -> its scancodes (layout independent: the key at that place on a US keyboard). "shift",
// "ctrl" and "alt" are both keys. False: not a key name.
bool ParseKey(const std::string& raw, std::vector<SDL_Scancode>& out) {
  const std::string k = Squash(raw);
  if (k.empty()) return false;
  static const struct {
    const char* name;
    SDL_Scancode a, b;
  } kNames[] = {
      {"space", SDL_SCANCODE_SPACE, SDL_SCANCODE_UNKNOWN},       {"return", SDL_SCANCODE_RETURN, SDL_SCANCODE_UNKNOWN},
      {"enter", SDL_SCANCODE_RETURN, SDL_SCANCODE_UNKNOWN},      {"kpenter", SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_UNKNOWN},
      {"numpadenter", SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_UNKNOWN}, {"escape", SDL_SCANCODE_ESCAPE, SDL_SCANCODE_UNKNOWN},
      {"esc", SDL_SCANCODE_ESCAPE, SDL_SCANCODE_UNKNOWN},        {"backspace", SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_UNKNOWN},
      {"tab", SDL_SCANCODE_TAB, SDL_SCANCODE_UNKNOWN},           {"lshift", SDL_SCANCODE_LSHIFT, SDL_SCANCODE_UNKNOWN},
      {"leftshift", SDL_SCANCODE_LSHIFT, SDL_SCANCODE_UNKNOWN},  {"rshift", SDL_SCANCODE_RSHIFT, SDL_SCANCODE_UNKNOWN},
      {"rightshift", SDL_SCANCODE_RSHIFT, SDL_SCANCODE_UNKNOWN}, {"shift", SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT},
      {"lctrl", SDL_SCANCODE_LCTRL, SDL_SCANCODE_UNKNOWN},       {"leftctrl", SDL_SCANCODE_LCTRL, SDL_SCANCODE_UNKNOWN},
      {"rctrl", SDL_SCANCODE_RCTRL, SDL_SCANCODE_UNKNOWN},       {"rightctrl", SDL_SCANCODE_RCTRL, SDL_SCANCODE_UNKNOWN},
      {"ctrl", SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL},          {"control", SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL},
      {"lalt", SDL_SCANCODE_LALT, SDL_SCANCODE_UNKNOWN},         {"ralt", SDL_SCANCODE_RALT, SDL_SCANCODE_UNKNOWN},
      {"alt", SDL_SCANCODE_LALT, SDL_SCANCODE_RALT},             {"up", SDL_SCANCODE_UP, SDL_SCANCODE_UNKNOWN},
      {"down", SDL_SCANCODE_DOWN, SDL_SCANCODE_UNKNOWN},         {"left", SDL_SCANCODE_LEFT, SDL_SCANCODE_UNKNOWN},
      {"right", SDL_SCANCODE_RIGHT, SDL_SCANCODE_UNKNOWN},       {"pageup", SDL_SCANCODE_PAGEUP, SDL_SCANCODE_UNKNOWN},
      {"pagedown", SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_UNKNOWN}, {"home", SDL_SCANCODE_HOME, SDL_SCANCODE_UNKNOWN},
      {"end", SDL_SCANCODE_END, SDL_SCANCODE_UNKNOWN},           {"insert", SDL_SCANCODE_INSERT, SDL_SCANCODE_UNKNOWN},
      {"delete", SDL_SCANCODE_DELETE, SDL_SCANCODE_UNKNOWN},     {"del", SDL_SCANCODE_DELETE, SDL_SCANCODE_UNKNOWN},
      {"capslock", SDL_SCANCODE_CAPSLOCK, SDL_SCANCODE_UNKNOWN}, {"comma", SDL_SCANCODE_COMMA, SDL_SCANCODE_UNKNOWN},
      {"period", SDL_SCANCODE_PERIOD, SDL_SCANCODE_UNKNOWN},     {"slash", SDL_SCANCODE_SLASH, SDL_SCANCODE_UNKNOWN},
      {"semicolon", SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_UNKNOWN}, {"apostrophe", SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_UNKNOWN},
      {"quote", SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_UNKNOWN},  {"leftbracket", SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_UNKNOWN},
      {"rightbracket", SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_UNKNOWN}, {"backslash", SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_UNKNOWN},
      {"minus", SDL_SCANCODE_MINUS, SDL_SCANCODE_UNKNOWN},       {"equals", SDL_SCANCODE_EQUALS, SDL_SCANCODE_UNKNOWN},
      {"grave", SDL_SCANCODE_GRAVE, SDL_SCANCODE_UNKNOWN},       {"backquote", SDL_SCANCODE_GRAVE, SDL_SCANCODE_UNKNOWN},
      {"kpplus", SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_UNKNOWN},    {"kpminus", SDL_SCANCODE_KP_MINUS, SDL_SCANCODE_UNKNOWN},
      {"kpmultiply", SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_UNKNOWN}, {"kpdivide", SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_UNKNOWN},
      {"kpperiod", SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_UNKNOWN},
  };
  for (const auto& n : kNames)
    if (k == n.name) {
      out.push_back(n.a);
      if (n.b != SDL_SCANCODE_UNKNOWN) out.push_back(n.b);
      return true;
    }
  if (k.size() == 1 && k[0] >= 'a' && k[0] <= 'z') return out.push_back(static_cast<SDL_Scancode>(SDL_SCANCODE_A + (k[0] - 'a'))), true;
  if (k.size() == 1 && k[0] >= '1' && k[0] <= '9') return out.push_back(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (k[0] - '1'))), true;
  if (k == "0") return out.push_back(SDL_SCANCODE_0), true;
  for (const char* pre : {"kp", "numpad", "keypad"}) {
    const size_t n = std::strlen(pre);
    if (k.size() == n + 1 && k.compare(0, n, pre) == 0 && k[n] >= '0' && k[n] <= '9')
      return out.push_back(k[n] == '0' ? SDL_SCANCODE_KP_0 : static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1 + (k[n] - '1'))), true;
  }
  if (k.size() >= 2 && k[0] == 'f') {
    const int f = std::atoi(k.c_str() + 1);
    if (f >= 1 && f <= 12 && k == "f" + std::to_string(f)) return out.push_back(static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + f - 1)), true;
  }
  const SDL_Scancode sc = SDL_GetScancodeFromName(Trim(raw).c_str());  // SDL's own names ("Keypad 5", ...)
  if (sc != SDL_SCANCODE_UNKNOWN) return out.push_back(sc), true;
  return false;
}

// "space, return" -> the action's keys (none: "none" or empty). Warns about names it does not know.
void ParseList(int action, const std::string& list, const std::string& where) {
  std::vector<SDL_Scancode> keys;
  size_t start = 0;
  while (start <= list.size()) {
    size_t end = list.find_first_of(",+", start);
    if (end == std::string::npos) end = list.size();
    const std::string name = Trim(list.substr(start, end - start));
    if (!name.empty() && Squash(name) != "none" && !ParseKey(name, keys))
      std::fprintf(stderr, "[keys] %s%sunknown key \"%s\" for %s\n", where.c_str(), where.empty() ? "" : ": ", name.c_str(),
                   kActions[action].name);
    start = end + 1;
  }
  for (SDL_Scancode k : keys)
    if (k == SDL_SCANCODE_F1 || k == SDL_SCANCODE_F2)
      std::fprintf(stderr, "[keys] %s%s%s on F1/F2 also opens the options / player 2's window\n", where.c_str(),
                   where.empty() ? "" : ": ", kActions[action].name);
  g_bind[action] = keys;
}

void LoadBindings() {
  if (g_bindingsLoaded) return;
  g_bindingsLoaded = true;
  for (int a = 0; a < kActionCount; ++a) ParseList(a, kActions[a].defaults, "");
  for (const GraphicsSettings::KeyLine& line : graphics_settings().keyLines) {
    const std::string name = Squash(line.action);
    int action = -1;
    for (int a = 0; a < kActionCount && action < 0; ++a)
      if (Squash(kActions[a].name) == name) action = a;
    // Short aliases.
    if (action < 0) {
      static const struct {
        const char* alias;
        int action;
      } kAlias[] = {{"up", kStickUp}, {"down", kStickDown}, {"left", kStickLeft}, {"right", kStickRight},
                    {"forward", kStickUp}, {"back", kStickDown}, {"slow", kWalk}, {"pause", kStart},
                    {"cancel", kB}, {"accept", kA}, {"select", kA}, {"nextsim", kY}, {"switchsim", kY}};
      for (const auto& al : kAlias)
        if (name == al.alias) action = al.action;
    }
    if (action < 0) {
      std::string known;
      for (const ActionDef& d : kActions) known += std::string(known.empty() ? "" : ", ") + d.name;
      std::fprintf(stderr, "[keys] %s: unknown action \"%s\" (known: %s)\n", line.where.c_str(), line.action.c_str(), known.c_str());
      continue;
    }
    ParseList(action, line.keys, line.where);
  }
}

// ---- input state (window 1's events) ----------------------------------------------------------------

bool g_keys[SDL_SCANCODE_COUNT] = {};
float g_mouseX = -1.0f, g_mouseY = -1.0f;  // window 1, its coordinates
bool g_mouseInside = false;
bool g_lDown = false, g_rDown = false, g_mDown = false;
bool g_lPressed = false, g_rReleasedClick = false;  // edges since the last frame
bool g_ignoreButtons = false;   // pressed while the options were open: ignored until released
float g_rDist = 0.0f;           // right button: motion since it went down
bool g_rDrag = false;           // ... past the threshold: a camera drag, not a click (B)
float g_dragX = 0.0f, g_dragY = 0.0f;  // camera drag this frame (pixels)
float g_wheel = 0.0f;           // notches this frame (positive: away from the user)
float g_motion = 0.0f;          // mouse motion since the keyboard's stick keys were last used (pixels)
bool g_mouseActive = false;     // the mouse leads the classic cursor (moved since the stick keys)

constexpr float kDragThreshold = 6.0f;   // pixels before a right-button press is a drag
constexpr float kActivateMotion = 4.0f;  // pixels of motion that hand the cursor to the mouse

KbmTick g_tick;
KbmTick g_tick1;  // PC mod: network play: pad port 2's record

// The per-frame state the pad presses are made from.
int g_aHold = 0, g_aWait = 0, g_bHold = 0;
bool g_aActive = false;
int g_wheelSteps = 0, g_wheelPress = 0, g_wheelGap = 0, g_wheelDir = 0;
bool g_cursorHidden = false;
bool g_lastCursorSet = false;   // tests: the last floor point sent
float g_lastCursor[2] = {};

unsigned g_tickCount = 0;

// The players' 3D views as last drawn.
struct View {
  bool valid = false;
  unsigned tick = 0;
  double inv[4][4] = {};
  float eye[3] = {}, dir[3] = {};
  float vp[4] = {};  // x0, y0, x1, y1 in the 640x480 logical frame
  float m[4][4] = {};
  bool windows = false;  // split=windows: window 1 shows the left half of the frame
  int strip = 0;         // ... (PC mod: network play: the half of this PC's player)
};
View g_views[2];
// Tests: the mouse, the floor point and the view when a screenshot was taken (the picture arrives a
// few frames later; the cross is drawn where the mouse was then).
struct ShotState {
  bool valid = false, inside = false, cursorSet = false;
  float mouseX = 0, mouseY = 0, cursor[2] = {};
  View view;
  unsigned tick = 0;
};
ShotState g_shot;

bool Invert(const float in[4][4], double out[4][4]) {
  double a[4][8];
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 8; ++c) a[r][c] = c < 4 ? in[r][c] : (c - 4 == r ? 1.0 : 0.0);
  for (int c = 0; c < 4; ++c) {
    int piv = c;
    for (int r = c + 1; r < 4; ++r)
      if (std::fabs(a[r][c]) > std::fabs(a[piv][c])) piv = r;
    if (std::fabs(a[piv][c]) < 1e-12) return false;
    if (piv != c)
      for (int k = 0; k < 8; ++k) std::swap(a[c][k], a[piv][k]);
    const double d = a[c][c];
    for (int k = 0; k < 8; ++k) a[c][k] /= d;
    for (int r = 0; r < 4; ++r)
      if (r != c) {
        const double f = a[r][c];
        for (int k = 0; k < 8; ++k) a[r][k] -= f * a[c][k];
      }
  }
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) out[r][c] = a[r][c + 4];
  return true;
}

// Window 1's size in its own coordinates (the mouse's).
bool WindowSize(float& w, float& h) {
  const AuroraWindowSize s = aurora_get_window_size();
  w = static_cast<float>(s.width), h = static_cast<float>(s.height);
  if (w <= 0.0f || h <= 0.0f) {
    w = static_cast<float>(s.native_fb_width), h = static_cast<float>(s.native_fb_height);
  }
  return w > 0.0f && h > 0.0f;
}

// Window position -> the 640x480 logical frame of view `v` (false: outside the picture).
bool WindowToLogical(const View& v, float mx, float my, float& lx, float& ly) {
  float w = 0, h = 0;
  if (!WindowSize(w, h)) return false;
  float u, t;
  if (v.windows) {
    u = mx / w * 0.5f + 0.5f * static_cast<float>(v.strip), t = my / h;  // window 1 = the frame's left half, stretched over it
  } else {
    const AuroraWindowSize s = aurora_get_window_size();
    float aspect = s.fb_width && s.fb_height ? static_cast<float>(s.fb_width) / static_cast<float>(s.fb_height) : w / h;
    float cw = w, ch = h;
    if (w / h > aspect) cw = h * aspect;
    else ch = w / aspect;
    u = (mx - (w - cw) * 0.5f) / cw, t = (my - (h - ch) * 0.5f) / ch;
  }
  if (u < 0.0f || u > 1.0f || t < 0.0f || t > 1.0f) return false;
  lx = u * 640.0f, ly = t * 480.0f;
  return true;
}

// Logical frame position -> window position (the test cross, at: moves).
bool LogicalToWindow(const View& v, float lx, float ly, float& mx, float& my) {
  float w = 0, h = 0;
  if (!WindowSize(w, h)) return false;
  const float u = lx / 640.0f, t = ly / 480.0f;
  if (v.windows) {
    mx = (u * 2.0f - static_cast<float>(v.strip)) * w, my = t * h;
  } else {
    const AuroraWindowSize s = aurora_get_window_size();
    float aspect = s.fb_width && s.fb_height ? static_cast<float>(s.fb_width) / static_cast<float>(s.fb_height) : w / h;
    float cw = w, ch = h;
    if (w / h > aspect) cw = h * aspect;
    else ch = w / aspect;
    mx = (w - cw) * 0.5f + u * cw, my = (h - ch) * 0.5f + t * ch;
  }
  return true;
}

bool ViewFresh(int player) {
  if (player < 0 || player > 1) return false;
  const View& v = g_views[player];
  return v.valid && g_tickCount - v.tick <= 3;
}

bool Trace();
bool WorldToWindow(int player, const float p[3], float& mx, float& my);

// The floor point (plane z) under window position (mx, my) in `player`'s view.
bool MouseToFloor(int player, float mx, float my, float z, float& wx, float& wy) {
  if (!ViewFresh(player)) return false;
  const View& v = g_views[player];
  float lx = 0, ly = 0;
  if (!WindowToLogical(v, mx, my, lx, ly)) return false;
  if (lx < v.vp[0] || lx > v.vp[2] || ly < v.vp[1] || ly > v.vp[3]) return false;  // another player's view
  const double hw = (v.vp[2] - v.vp[0]) * 0.5, hh = (v.vp[3] - v.vp[1]) * 0.5;
  if (hw <= 0.0 || hh <= 0.0) return false;
  const double nx = (lx - (v.vp[0] + hw)) / hw, ny = -(ly - (v.vp[1] + hh)) / hh;
  const double in[4] = {nx, ny, 0.5, 1.0};
  double p[4] = {};
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r) p[c] += in[r] * v.inv[r][c];
  if (std::fabs(p[3]) < 1e-12) return false;
  double d[3] = {p[0] / p[3] - v.eye[0], p[1] / p[3] - v.eye[1], p[2] / p[3] - v.eye[2]};
  if (d[0] * v.dir[0] + d[1] * v.dir[1] + d[2] * v.dir[2] < 0.0) d[0] = -d[0], d[1] = -d[1], d[2] = -d[2];
  if (d[2] > -1e-9) return false;  // looking up: no floor there
  // An object in front of the floor (its box) takes the cursor: pointing at a table top or a phone on
  // the wall picks it, not the floor behind it.
  const float eye[3] = {v.eye[0], v.eye[1], v.eye[2]};
  const float dir[3] = {static_cast<float>(d[0]), static_cast<float>(d[1]), static_cast<float>(d[2])};
  if (kbm_pick(player, eye, dir, z, wx, wy)) return true;
  const double t = (z - v.eye[2]) / d[2];
  if (t <= 0.0) return false;
  wx = static_cast<float>(v.eye[0] + t * d[0]);
  wy = static_cast<float>(v.eye[1] + t * d[1]);
  if (Trace() && g_tickCount % 30 == 0) {
    double q[4] = {};
    const double in2[4] = {nx, ny, -0.5, 1.0};
    for (int c = 0; c < 4; ++c)
      for (int r = 0; r < 4; ++r) q[c] += in2[r] * v.inv[r][c];
    const float fp[3] = {wx, wy, z};
    float bx = -1, by = -1;
    WorldToWindow(player, fp, bx, by);
    std::fprintf(stderr, "[kbm] ray: mouse %.0f,%.0f logical %.1f,%.1f ndc %.3f,%.3f eye %.3f,%.3f,%.3f P+ %.3f,%.3f,%.3f (w %.3f) P- %.3f,%.3f,%.3f (w %.3f) floor %.3f,%.3f back %.1f,%.1f\n",
                 mx, my, lx, ly, nx, ny, v.eye[0], v.eye[1], v.eye[2], p[0] / p[3], p[1] / p[3], p[2] / p[3], p[3], q[0] / q[3],
                 q[1] / q[3], q[2] / q[3], q[3], wx, wy, bx, by);
  }
  return true;
}

// World point -> window position in `player`'s view (tests).
bool WorldToWindow(int player, const float p[3], float& mx, float& my) {
  if (!ViewFresh(player)) return false;
  const View& v = g_views[player];
  double c[4] = {};
  const double in[4] = {p[0], p[1], p[2], 1.0};
  for (int k = 0; k < 4; ++k)
    for (int r = 0; r < 4; ++r) c[k] += in[r] * v.m[r][k];
  if (c[3] <= 1e-9) return false;
  const double nx = c[0] / c[3], ny = c[1] / c[3];
  const double hw = (v.vp[2] - v.vp[0]) * 0.5, hh = (v.vp[3] - v.vp[1]) * 0.5;
  const float lx = static_cast<float>(v.vp[0] + hw + nx * hw), ly = static_cast<float>(v.vp[1] + hh - ny * hh);
  return LogicalToWindow(v, lx, ly, mx, my);
}

bool Held(int action) {
  for (SDL_Scancode k : g_bind[action])
    if (k > SDL_SCANCODE_UNKNOWN && k < SDL_SCANCODE_COUNT && g_keys[k]) return true;
  return false;
}

void ReleaseAll() {
  std::memset(g_keys, 0, sizeof g_keys);
  g_lDown = g_rDown = g_mDown = false;
  g_rDrag = false;
  g_rDist = 0.0f;
}

bool Trace() {
  static const bool on = std::getenv("SIMMY_KBM_TRACE") && *std::getenv("SIMMY_KBM_TRACE") != '0';
  return on;
}

// ---- the test script (SIMMY_KBM_TEST) ----------------------------------------------------------------

struct TestStep {
  unsigned frame;
  std::string action;
};
std::vector<TestStep> g_test;
bool g_testOn = false;
size_t g_testNext = 0;
struct Glide {
  float x0, y0, x1, y1;
  int frames, done;
  bool relative;
};
std::vector<Glide> g_glides;
std::vector<std::pair<unsigned, std::string>> g_testLater;  // releases of tap/click

void ParseTest() {
  const char* env = std::getenv("SIMMY_KBM_TEST");
  if (!env || !*env) return;
  g_testOn = true;
  std::string all = env;
  size_t start = 0;
  while (start < all.size()) {
    size_t end = all.find(';', start);
    if (end == std::string::npos) end = all.size();
    const std::string item = Trim(all.substr(start, end - start));
    const size_t colon = item.find(':');
    if (colon != std::string::npos) {
      std::string f = item.substr(0, colon);
      if (!f.empty() && f[0] == 'd') f.erase(0, 1);
      g_test.push_back({static_cast<unsigned>(std::strtoul(f.c_str(), nullptr, 10)), item.substr(colon + 1)});
    }
    start = end + 1;
  }
  std::stable_sort(g_test.begin(), g_test.end(), [](const TestStep& a, const TestStep& b) { return a.frame < b.frame; });
}

void Synth(const SDL_Event& e);

void SynthKey(const std::string& name, bool down) {
  std::vector<SDL_Scancode> keys;
  if (!ParseKey(name, keys) || keys.empty()) {
    std::fprintf(stderr, "[kbm-test] unknown key %s\n", name.c_str());
    return;
  }
  SDL_Event e{};
  e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  e.key.scancode = keys[0];
  e.key.down = down;
  Synth(e);
}
void SynthMove(float x, float y) {
  SDL_Event e{};
  e.type = SDL_EVENT_MOUSE_MOTION;
  e.motion.xrel = x - (g_mouseX < 0 ? x : g_mouseX);
  e.motion.yrel = y - (g_mouseY < 0 ? y : g_mouseY);
  e.motion.x = x, e.motion.y = y;
  if (!g_mouseInside) {
    SDL_Event enter{};
    enter.type = SDL_EVENT_WINDOW_MOUSE_ENTER;
    Synth(enter);
  }
  Synth(e);
}
void SynthButton(int button, bool down) {
  SDL_Event e{};
  e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
  e.button.button = static_cast<Uint8>(button);
  e.button.down = down;
  e.button.x = g_mouseX, e.button.y = g_mouseY;
  Synth(e);
}

bool TestObjectPos(const std::string& what, float out[3]);  // below (game objects)

void RunTestStep(const std::string& a, unsigned frame) {
  std::fprintf(stderr, "[kbm-test] frame %u: %s\n", frame, a.c_str());
  const size_t colon = a.find(':');
  const std::string verb = a.substr(0, colon), arg = colon == std::string::npos ? std::string() : a.substr(colon + 1);
  int frames = 1;
  std::string val = arg;
  if (const size_t slash = arg.find('/'); slash != std::string::npos) frames = std::max(1, std::atoi(arg.c_str() + slash + 1)), val = arg.substr(0, slash);
  float x = 0, y = 0;
  std::sscanf(val.c_str(), "%f,%f", &x, &y);
  if (verb == "key") SynthKey(val, true);
  else if (verb == "keyup") SynthKey(val, false);
  else if (verb == "tap") {
    SynthKey(val, true);
    g_testLater.emplace_back(frame + static_cast<unsigned>(std::max(frames, 3)), "keyup:" + val);
  } else if (verb == "move") SynthMove(x, y);
  else if (verb == "glide") g_glides.push_back({g_mouseX < 0 ? x : g_mouseX, g_mouseY < 0 ? y : g_mouseY, x, y, frames, 0, false});
  else if (verb == "drag") g_glides.push_back({0, 0, x, y, frames, 0, true});
  else if (verb == "at") {
    float p[3] = {};
    float mx = 0, my = 0;
    if (TestObjectPos(val, p) && WorldToWindow(0, p, mx, my)) {
      std::fprintf(stderr, "[kbm-test] %s at %.2f %.2f %.2f -> window %.1f, %.1f\n", val.c_str(), p[0], p[1], p[2], mx, my);
      SynthMove(mx, my);
    } else {
      std::fprintf(stderr, "[kbm-test] %s: not found or not in view\n", val.c_str());
    }
  } else if (verb == "ldown") SynthButton(SDL_BUTTON_LEFT, true);
  else if (verb == "lup") SynthButton(SDL_BUTTON_LEFT, false);
  else if (verb == "lclick") {
    SynthButton(SDL_BUTTON_LEFT, true);
    g_testLater.emplace_back(frame + 4, "lup");
  } else if (verb == "rdown") SynthButton(SDL_BUTTON_RIGHT, true);
  else if (verb == "rup") SynthButton(SDL_BUTTON_RIGHT, false);
  else if (verb == "rclick") {
    SynthButton(SDL_BUTTON_RIGHT, true);
    g_testLater.emplace_back(frame + 4, "rup");
  } else if (verb == "mdown") SynthButton(SDL_BUTTON_MIDDLE, true);
  else if (verb == "mup") SynthButton(SDL_BUTTON_MIDDLE, false);
  else if (verb == "wheel") {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.y = x;
    Synth(e);
  } else if (verb == "mouseout") {
    SDL_Event e{};
    e.type = SDL_EVENT_WINDOW_MOUSE_LEAVE;
    Synth(e);
  } else {
    std::fprintf(stderr, "[kbm-test] unknown action %s\n", a.c_str());
  }
}

void RunTest() {
  if (!g_testOn) return;
  const unsigned frame = platform_dev_frame();
  if (!frame) return;
  while (g_testNext < g_test.size() && g_test[g_testNext].frame <= frame) RunTestStep(g_test[g_testNext++].action, frame);
  for (size_t i = 0; i < g_testLater.size();) {
    if (g_testLater[i].first <= frame) {
      const std::string a = g_testLater[i].second;
      g_testLater.erase(g_testLater.begin() + static_cast<long>(i));
      RunTestStep(a, frame);
    } else {
      ++i;
    }
  }
  for (size_t i = 0; i < g_glides.size();) {
    Glide& gl = g_glides[i];
    ++gl.done;
    if (gl.relative) {
      SynthMove(g_mouseX + gl.x1 / static_cast<float>(gl.frames), g_mouseY + gl.y1 / static_cast<float>(gl.frames));
    } else {
      const float f = static_cast<float>(gl.done) / static_cast<float>(gl.frames);
      SynthMove(gl.x0 + (gl.x1 - gl.x0) * f, gl.y0 + (gl.y1 - gl.y0) * f);
    }
    if (gl.done >= gl.frames) g_glides.erase(g_glides.begin() + static_cast<long>(i));
    else ++i;
  }
}

// ---- events -------------------------------------------------------------------------------------------

void Handle(const SDL_Event& e) {
  switch (e.type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
    if (e.key.scancode > SDL_SCANCODE_UNKNOWN && e.key.scancode < SDL_SCANCODE_COUNT) g_keys[e.key.scancode] = e.type == SDL_EVENT_KEY_DOWN;
    break;
  case SDL_EVENT_MOUSE_MOTION: {
    g_mouseX = e.motion.x, g_mouseY = e.motion.y;
    g_mouseInside = true;
    const float d = std::sqrt(e.motion.xrel * e.motion.xrel + e.motion.yrel * e.motion.yrel);
    g_motion += d;
    if (g_rDown && !g_ignoreButtons) {
      g_rDist += d;
      if (g_rDist > kDragThreshold) g_rDrag = true;
    }
    if ((g_rDrag && g_rDown) || g_mDown) g_dragX += e.motion.xrel, g_dragY += e.motion.yrel;
    break;
  }
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    const bool down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    if (down && overlay_is_open()) g_ignoreButtons = true;
    if (e.button.button == SDL_BUTTON_LEFT) {
      if (down && !g_lDown && !g_ignoreButtons) g_lPressed = true;
      g_lDown = down;
    } else if (e.button.button == SDL_BUTTON_RIGHT) {
      if (!down && g_rDown && !g_rDrag && !g_ignoreButtons) g_rReleasedClick = true;
      g_rDown = down;
      g_rDist = 0.0f;
      g_rDrag = false;
    } else if (e.button.button == SDL_BUTTON_MIDDLE) {
      g_mDown = down && !g_ignoreButtons;
    }
    if (!g_lDown && !g_rDown && !g_mDown) g_ignoreButtons = false;
    break;
  }
  case SDL_EVENT_MOUSE_WHEEL: {
    float y = e.wheel.y;
    if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) y = -y;
    if (!overlay_is_open()) g_wheel += y;
    break;
  }
  case SDL_EVENT_WINDOW_MOUSE_ENTER: g_mouseInside = true; break;
  case SDL_EVENT_WINDOW_MOUSE_LEAVE: g_mouseInside = false; break;
  case SDL_EVENT_WINDOW_FOCUS_LOST: ReleaseAll(); break;
  default: break;
  }
}

void Synth(const SDL_Event& e) { Handle(e); }

void SetCursorHidden(bool hide) {
  if (hide == g_cursorHidden || dev_options().offscreen) return;
  g_cursorHidden = hide;
  if (hide) SDL_HideCursor();
  else SDL_ShowCursor();
}

// Game objects for the test's "at:" (the object's or person's position as drawn).
bool TestObjectPos(const std::string& what, float out[3]) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module || what.empty()) return false;
  cXObjectImpl* target = nullptr;
  if (what[0] == 'g') {
    const unsigned guid = static_cast<unsigned>(std::strtoul(what.c_str() + 1, nullptr, 16));
    for (cXObjectImpl* o = module->m_first; o && !target; o = o->m_nextObject)
      if (o->m_def && objdef::GUID(o->m_def) == guid) target = o;
  } else {
    cXObject* x = module->GetObjectFromID(static_cast<short>(std::atoi(what.c_str() + (what[0] == '#' ? 1 : 0))));
    target = x ? x->GetObjectImplementation() : nullptr;
  }
  if (!target) return false;
  // Tile coordinates: x along the tiles' y (location[1]), as the classic cursor and the camera use them.
  out[0] = static_cast<float>(target->m_location[1]) / 16.0f;
  out[1] = static_cast<float>(target->m_location[0]) / 16.0f;
  out[2] = 0.0f;
  if (ISimInstance* inst = target->m_instance) {
    const float* b = inst->m_bounds;
    if (b[3] > b[0] || b[4] > b[1]) out[0] = (b[0] + b[3]) * 0.5f, out[1] = (b[1] + b[4]) * 0.5f;
  }
  return true;
}

}  // namespace

// ---- the interface ------------------------------------------------------------------------------------

void kbm_init() {
  LoadBindings();  // (warns about unknown [keys] names at start)
  const char* modes[] = {"auto (keyboard + mouse and controller)", "controller", "keyboard_mouse"};
  const int m = graphics_settings().p1Input;
  std::fprintf(stderr, "[kbm] player 1: %s\n", modes[m >= 0 && m <= 2 ? m : 0]);
}

void kbm_handle_event(const SDL_Event& e) {
  if (g_testOn) {
    // Tests: the synthetic events only (a real window may also be there).
    return;
  }
  Handle(e);
}

int kbm_p1_input() { return graphics_settings().p1Input; }

const KbmTick& kbm_tick() { return g_tick; }
KbmTick& kbm_tick_for_det() { return g_tick; }
// PC mod: network play: port 2's record (the joining PC's keyboard and mouse, set by pc_net.cpp or a
// replay; empty otherwise).
const KbmTick& kbm_tick_port(int port) { return port == 1 ? g_tick1 : g_tick; }
KbmTick& kbm_tick_port_for_det(int port) { return port == 1 ? g_tick1 : g_tick; }
int kbm_local_port() { return net_local_slot(); }

void kbm_note_view(int player, const float m[4][4], const float eye[3], const float dir[3], const float viewport[4]) {
  if (player < 0 || player > 1) return;
  View& v = g_views[player];
  std::memcpy(v.m, m, sizeof v.m);
  v.valid = Invert(m, v.inv);
  v.tick = g_tickCount;
  for (int i = 0; i < 3; ++i) v.eye[i] = eye[i], v.dir[i] = dir[i];
  for (int i = 0; i < 4; ++i) v.vp[i] = viewport[i];
  v.windows = split_windows_frame();
  v.strip = net_view_player() == 1 ? 1 : 0;  // PC mod: network play: the joining PC shows the right half
  static int s_logged = 0;
  if (Trace() && s_logged < 3 && player == 0) {
    ++s_logged;
    std::fprintf(stderr, "[kbm] view p%d: vp %.1f,%.1f..%.1f,%.1f eye %.2f %.2f %.2f dir %.2f %.2f %.2f windows %d\n", player + 1,
                 v.vp[0], v.vp[1], v.vp[2], v.vp[3], eye[0], eye[1], eye[2], dir[0], dir[1], dir[2], v.windows ? 1 : 0);
  }
}

void kbm_pads(PADStatus* pads) {
  ++g_tickCount;
  g_tick = KbmTick{};
  g_tick1 = KbmTick{};
  if (platform_headless()) return;
  LoadBindings();
  static bool s_testParsed = false;
  if (!s_testParsed) s_testParsed = true, ParseTest();
  RunTest();
  const GraphicsSettings& gs = graphics_settings();
  const int mode = gs.p1Input;
  // Taken every frame (also when unused), so that edges never pile up.
  const float wheel = g_wheel, dragX = g_dragX, dragY = g_dragY, motion = g_motion;
  const bool lPressed = g_lPressed, rClick = g_rReleasedClick;
  g_wheel = g_dragX = g_dragY = 0.0f;
  g_lPressed = g_rReleasedClick = false;
  if (mode == 1) return;  // controller only
  if (overlay_is_open()) {
    // The options have the keyboard and the mouse; nothing reaches the game (the pads are neutralised
    // by overlay_filter_pads, held keys stay latched until released).
    g_aHold = g_aWait = g_bHold = 0;
    g_aActive = false;
    g_wheelSteps = g_wheelPress = g_wheelGap = 0;
    SetCursorHidden(false);
    pads[0].err = PAD_ERR_NONE;
    return;
  }
  KbmGame game;
  kbm_game_state(game);
  const SDL_Keymod mods = g_testOn ? SDL_KMOD_NONE : SDL_GetModState();
  const bool altHeld = (mods & (SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0;  // Alt+Enter and the like are not game keys

  PADStatus k{};
  auto held = [&](int a) { return !altHeld && Held(a); };
  // The main stick: full deflection (the GC's runs), half with the walk key.
  const float sx = (held(kStickRight) ? 1.0f : 0.0f) - (held(kStickLeft) ? 1.0f : 0.0f);
  const float sy = (held(kStickUp) ? 1.0f : 0.0f) - (held(kStickDown) ? 1.0f : 0.0f);
  const bool stickKeys = sx != 0.0f || sy != 0.0f;
  if (stickKeys) {
    if (held(kWalk)) {
      // Half the game's full scale (40): a walk, also diagonally (0.5 x 40 = 20 per axis).
      k.stickX = static_cast<s8>(sx * 20.0f), k.stickY = static_cast<s8>(sy * 20.0f);
    } else {
      k.stickX = static_cast<s8>(sx * 127.0f), k.stickY = static_cast<s8>(sy * 127.0f);
    }
  }
  const float cx = (held(kCRight) ? 1.0f : 0.0f) - (held(kCLeft) ? 1.0f : 0.0f);
  const float cy = (held(kCUp) ? 1.0f : 0.0f) - (held(kCDown) ? 1.0f : 0.0f);
  k.substickX = static_cast<s8>(cx * 127.0f), k.substickY = static_cast<s8>(cy * 127.0f);
  static const struct {
    int action;
    u16 bit;
  } kButtons[] = {{kA, PAD_BUTTON_A},       {kB, PAD_BUTTON_B},         {kX, PAD_BUTTON_X},        {kY, PAD_BUTTON_Y},
                  {kZ, PAD_TRIGGER_Z},      {kStart, PAD_BUTTON_START}, {kL, PAD_TRIGGER_L},       {kR, PAD_TRIGGER_R},
                  {kDUp, PAD_BUTTON_UP},    {kDDown, PAD_BUTTON_DOWN},  {kDLeft, PAD_BUTTON_LEFT}, {kDRight, PAD_BUTTON_RIGHT}};
  for (const auto& b : kButtons)
    if (held(b.action)) k.button = static_cast<u16>(k.button | b.bit);
  if (k.button & PAD_TRIGGER_L) k.triggerLeft = 255;
  if (k.button & PAD_TRIGGER_R) k.triggerRight = 255;

  // Into port 1: alone (keyboard_mouse, or no controller), else merged with the controller's.
  PADStatus& p = pads[0];
  if (mode == 2 || p.err != PAD_ERR_NONE) {
    p = k;
  } else {
    p.button = static_cast<u16>(p.button | k.button);
    if (k.stickX || k.stickY) p.stickX = k.stickX, p.stickY = k.stickY;
    if (k.substickX || k.substickY) p.substickX = k.substickX, p.substickY = k.substickY;
    p.triggerLeft = std::max(p.triggerLeft, k.triggerLeft);
    p.triggerRight = std::max(p.triggerRight, k.triggerRight);
  }
  p.err = PAD_ERR_NONE;

  // The mouse leads the classic cursor once it moves (or clicks) after the stick (keys or controller)
  // or Z was used; the stick hands a mouse-led classic control back to direct control.
  const bool stickUsed = std::abs(static_cast<int>(p.stickX)) > 10 || std::abs(static_cast<int>(p.stickY)) > 10;
  if (stickUsed || (p.button & PAD_TRIGGER_Z)) {
    g_mouseActive = false;
    g_motion = 0.0f;
  }
  if (stickUsed) g_tick.flags |= kKbmKeysMoved;
  else if (motion > 0.0f && g_motion > kActivateMotion && !(p.button & PAD_TRIGGER_Z)) g_mouseActive = true;
  if (lPressed) g_mouseActive = true;
  // The floor point under the mouse, when it can lead the cursor there.
  float wx = 0, wy = 0;
  const bool overView = g_mouseInside && game.live && MouseToFloor(game.player, g_mouseX, g_mouseY, game.cursorZ, wx, wy);
  // (Not while dragging the camera round: the cursor stays where it was until the button is let go.)
  const bool dragging = g_mDown || (g_rDown && g_rDrag);
  const bool cursor = overView && g_mouseActive && game.cursorFree && !stickUsed && !dragging;
  if (cursor) {
    g_tick.flags |= kKbmCursor;
    g_tick.cx = wx, g_tick.cy = wy;
    g_lastCursorSet = true;
    g_lastCursor[0] = wx, g_lastCursor[1] = wy;
  }
  k.button = 0;  // from here: the mouse's presses
  // Left button = A: held while the button is, at least 3 frames. Over the 3D view in direct control it
  // first hands the player to the classic control (the cursor at the mouse) and presses A once that
  // takes it (at most 12 frames later), so A opens the menu at the cursor rather than the sim's.
  if (lPressed) {
    if (cursor && !game.classicReady) g_aWait = 12;
    else g_aHold = 3, g_aActive = true;
  }
  if (g_aWait > 0) {
    if (game.classicReady || --g_aWait == 0) g_aWait = 0, g_aHold = 3, g_aActive = true;
  }
  if (g_aActive) {
    if (g_aHold > 0 || g_lDown) k.button = static_cast<u16>(k.button | PAD_BUTTON_A);
    if (g_aHold > 0) --g_aHold;
    if (g_aHold == 0 && !g_lDown) g_aActive = false;
  }
  // Right click (no drag) = B, 3 frames.
  if (rClick) g_bHold = 3;
  if (g_bHold > 0) {
    k.button = static_cast<u16>(k.button | PAD_BUTTON_B);
    --g_bHold;
  }
  // The wheel: zoom where the camera takes the C stick, else d-pad up / down (one press a notch).
  const float wheelSign = gs.mouseInvertWheel ? -1.0f : 1.0f;
  if (game.cameraFree) {
    if (wheel != 0.0f) g_tick.zoom += wheel * wheelSign * gs.mouseZoomSpeed;
    g_wheelSteps = 0;
  } else if (wheel != 0.0f) {
    static float s_frac = 0.0f;
    s_frac += wheel;
    const int whole = static_cast<int>(s_frac);
    s_frac -= static_cast<float>(whole);
    g_wheelSteps = std::max(-8, std::min(8, g_wheelSteps + whole));
  }
  if (g_wheelPress > 0) {
    k.button = static_cast<u16>(k.button | (g_wheelDir > 0 ? PAD_BUTTON_UP : PAD_BUTTON_DOWN));
    if (--g_wheelPress == 0) g_wheelGap = 2;
  } else if (g_wheelGap > 0) {
    --g_wheelGap;
  } else if (g_wheelSteps != 0) {
    g_wheelDir = g_wheelSteps > 0 ? 1 : -1;
    g_wheelSteps -= g_wheelDir;
    g_wheelPress = 2;
    k.button = static_cast<u16>(k.button | (g_wheelDir > 0 ? PAD_BUTTON_UP : PAD_BUTTON_DOWN));
  }
  // Drags (middle button, or right button past the threshold): turn the camera with x, zoom with y.
  if (dragX != 0.0f || dragY != 0.0f) {
    g_tick.rot += -dragX * 0.3f * gs.mouseRotateSpeed * (gs.mouseInvertX ? -1.0f : 1.0f);
    g_tick.zoom += -dragY * 0.02f * gs.mouseRotateSpeed * (gs.mouseInvertY ? -1.0f : 1.0f);
  }
  // Panning (the camera while the mouse leads the classic cursor): keys and, if on, the screen edges.
  g_tick.panX = (held(kPanRight) ? 1.0f : 0.0f) - (held(kPanLeft) ? 1.0f : 0.0f);
  g_tick.panY = (held(kPanUp) ? 1.0f : 0.0f) - (held(kPanDown) ? 1.0f : 0.0f);
  const bool edgePan = gs.mouseEdgePan == 1 || (gs.mouseEdgePan == 2 && settings_fullscreen_now());
  float ww = 0, wh = 0;
  if (edgePan && g_mouseInside && game.mouseClassic && WindowSize(ww, wh)) {
    const float edge = 6.0f;
    if (g_mouseX <= edge) g_tick.panX = -1.0f;
    if (g_mouseX >= ww - 1.0f - edge) g_tick.panX = 1.0f;
    if (g_mouseY <= edge) g_tick.panY = 1.0f;
    if (g_mouseY >= wh - 1.0f - edge) g_tick.panY = -1.0f;
  }

  p.button = static_cast<u16>(p.button | k.button);

  // The system cursor: hidden while it points into the 3D view the game's cursor follows.
  SetCursorHidden(overView && game.mouseClassic && game.cursorFree);

  if (Trace()) {
    static KbmTick s_last;
    static u16 s_lastButtons = 0;
    static s8 s_lastStick[2] = {};
    if (!(g_tick == s_last) || p.button != s_lastButtons || p.stickX != s_lastStick[0] || p.stickY != s_lastStick[1])
      std::fprintf(stderr,
                   "[kbm] frame %u: buttons %04x stick %d,%d c %d,%d flags %x cursor %.2f,%.2f rot %.2f zoom %.2f pan %.0f,%.0f "
                   "(player %d live %d cam %d cur %d ready %d mouse %d; mouse %.0f,%.0f)\n",
                   platform_dev_frame(), p.button, p.stickX, p.stickY, p.substickX, p.substickY, g_tick.flags, g_tick.cx,
                   g_tick.cy, g_tick.rot, g_tick.zoom, g_tick.panX, g_tick.panY, game.player + 1, game.live ? 1 : 0,
                   game.cameraFree ? 1 : 0, game.cursorFree ? 1 : 0, game.classicReady ? 1 : 0, game.mouseClassic ? 1 : 0,
                   g_mouseX, g_mouseY);
    s_last = g_tick;
    s_lastButtons = p.button;
    s_lastStick[0] = p.stickX, s_lastStick[1] = p.stickY;
  }
}

// The keys of an action as text, for the F1 overlay ("W", "Space, Return").
std::string kbm_binding_text(int action) {
  LoadBindings();
  if (action < 0 || action >= kActionCount) return "";
  std::string out;
  for (SDL_Scancode k : g_bind[action]) {
    const char* n = SDL_GetScancodeName(k);
    if (!n || !*n) continue;
    if (!out.empty()) out += ", ";
    out += n;
  }
  return out.empty() ? "-" : out;
}
const char* kbm_action_label(int action) { return action >= 0 && action < kActionCount ? kActions[action].label : nullptr; }
int kbm_action_count() { return kActionCount; }

// Offscreen test shots: the mouse as a small cross (there is no system cursor to see).
void kbm_shot_requested() {
  g_shot.valid = true;
  g_shot.inside = g_mouseInside && g_mouseX >= 0.0f;
  g_shot.mouseX = g_mouseX, g_shot.mouseY = g_mouseY;
  g_shot.cursorSet = g_lastCursorSet;
  g_shot.cursor[0] = g_lastCursor[0], g_shot.cursor[1] = g_lastCursor[1];
  g_shot.view = g_views[0];
  g_shot.tick = g_tickCount;
}

void kbm_composite_shot(unsigned char* rgba, unsigned width, unsigned height) {
  if (!g_testOn || !g_shot.valid || !g_shot.inside) return;
  g_shot.valid = false;
  float w = 0, h = 0;
  if (!WindowSize(w, h)) return;
  // Window position -> shot pixel. The shot is the frame (EFB): window 1 shows all of it (fitted), or
  // (split=windows) its left half.
  const View& v = g_shot.view;
  auto toShot = [&](float mx, float my, int& x, int& y) {
    float u = mx / w, t = my / h;
    if (v.valid && v.windows) {
      u = u * 0.5f + 0.5f * static_cast<float>(v.strip);
    } else {
      const AuroraWindowSize s = aurora_get_window_size();
      const float aspect = s.fb_width && s.fb_height ? static_cast<float>(s.fb_width) / static_cast<float>(s.fb_height) : w / h;
      float cw = w, ch = h;
      if (w / h > aspect) cw = h * aspect;
      else ch = w / aspect;
      u = (mx - (w - cw) * 0.5f) / cw, t = (my - (h - ch) * 0.5f) / ch;
    }
    x = static_cast<int>(u * static_cast<float>(width)), y = static_cast<int>(t * static_cast<float>(height));
  };
  auto put = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) return;
    unsigned char* q = rgba + (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4;
    q[0] = r, q[1] = g, q[2] = b, q[3] = 255;
  };
  const int arm = static_cast<int>(height / 40) + 4;
  // The last floor point sent (the cursor's foot) projected back through the view of then: a small green cross.
  if (g_shot.cursorSet) {
    const View saved = g_views[0];
    const unsigned savedTick = g_tickCount;
    g_views[0] = g_shot.view, g_tickCount = g_shot.tick;
    const float p[3] = {g_shot.cursor[0], g_shot.cursor[1], 0.0f};
    float mx = 0, my = 0;
    const bool ok = WorldToWindow(0, p, mx, my);
    g_views[0] = saved, g_tickCount = savedTick;
    int gx = 0, gy = 0;
    if (ok) {
      toShot(mx, my, gx, gy);
      for (int d = -arm / 2; d <= arm / 2; ++d)
        for (int o = 0; o <= 1; ++o) put(gx + d, gy + o, 40, 255, 40), put(gx + o, gy + d, 40, 255, 40);
    }
  }
  // The mouse: a red cross with a dark outline.
  int cx = 0, cy = 0;
  toShot(g_shot.mouseX, g_shot.mouseY, cx, cy);
  for (int d = -arm; d <= arm; ++d)
    for (int o = -1; o <= 1; ++o) {
      const bool core = o == 0;
      put(cx + d, cy + o, core ? 255 : 0, core ? 40 : 0, core ? 40 : 0);
      put(cx + o, cy + d, core ? 255 : 0, core ? 40 : 0, core ? 40 : 0);
    }
}

}  // namespace simmy
