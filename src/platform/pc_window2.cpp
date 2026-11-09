// PC: the second player's window (co-op display split=windows, see pc_splitscreen.cpp).
//
// Aurora owns one SDL window and presents the EFB to it. This file opens a second SDL window, makes a
// WebGPU surface for it on aurora's device and registers it as an extra aurora present target. Each
// frame (window2_present, just before aurora_end_frame):
//   - in a two-player split frame the EFB holds player 1's half on the left and player 2's on the
//     right (pc_splitscreen.cpp sizes it at the windows' resolution): aurora's primary present shows
//     the left half in window 1 and present_to shows the right half here (aurora::gfx::set_present_crop,
//     the port's aurora patch, cmake/aurora_patches);
//   - otherwise (front end, Create-a-Sim, loading, one player, build/buy) this window mirrors window 1,
//     aspect fitted, so the second player always sees what the game shows.
// The present here is never vsynced (mailbox where the driver has it, else immediate): window 1 keeps
// the vsync setting and paces the game; waiting for a second swap chain could halve the frame rate.
//
// Input stays as it was: both pads are read whichever window has focus (gamepads are not window
// events; background input is allowed while the window is open); keyboard, mouse and F1 belong to
// window 1. Closing this window only hides it (window 1 then shows the split view as split=wide does);
// F2 in window 1 or the F1 options show it again. Closing window 1 quits as before.
// Its size, position, display and fullscreen state are written to simmy.ini (window2, window2_pos,
// window2_display, fullscreen2) when the game quits or the window is closed or the mode is left.
//
// Offscreen runs (tests) have no windows: a "virtual" second window of the window2= size (default
// the first window's) takes part in the layout, and screenshots also write what each window would show.
//
// PC mod: 4 players. With extra free-play players (src/game/mp4.cpp) the frame is cut into one strip
// per window (split_window_count) and this file also keeps windows 3 and 4 ("SIMMY - Player N",
// window 1's size, placed under windows 1 and 2): opened when their player joins, closed when the
// player drops out. Closing one asks the game to drop that player (split_window_take_close_request).
// They are not remembered in simmy.ini, are never vsynced, and mirror window 1 outside split frames.
#include "simmy/version.hpp"
#include "pc_window2.hpp"

#include "simmy/platform.hpp"
#include "pc_settings.hpp"

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_video.h>
#include <aurora/aurora.h>
#include <aurora/gfx.hpp>
#include <aurora/webgpu.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace simmy {
namespace {
// PC mod: 4 players: windows 3 and 4 (index 2, 3). Window 2 keeps the globals below.
struct ExtraWindow {
  SDL_Window* window = nullptr;
  aurora::gfx::PresentTargetId target = aurora::gfx::InvalidPresentTarget;
  bool failed = false;
  bool cropped = false;
  bool closeRequested = false;
  float crop = -1.0f;  // the strip shown (u0), -1 none
};
constexpr int kWindowsMax = 4;  // kMaxPlayers (port.hpp)
ExtraWindow g_extra[kWindowsMax];  // [2] and [3] used
SDL_Window* g_window = nullptr;
aurora::gfx::PresentTargetId g_target = aurora::gfx::InvalidPresentTarget;
bool g_failed = false;        // the window or its surface could not be made: not tried again
bool g_hidden = false;        // closed (or hidden) by the player
bool g_primaryCropped = false;
bool g_cropped = false;
bool g_fullscreen = false;
bool g_backgroundInput = false;
// The geometry last read from or written to simmy.ini (written again only if it changed).
GraphicsSettings g_saved;

bool Offscreen() { return dev_options().offscreen; }
bool Wanted() { return split_mode() == SplitMode::Windows && !platform_headless(); }

// The SDL display list index of a display (-1 if not found).
int DisplayIndex(SDL_DisplayID id) {
  int n = 0;
  SDL_DisplayID* list = SDL_GetDisplays(&n);
  int found = -1;
  for (int i = 0; list && i < n; ++i)
    if (list[i] == id) found = i;
  SDL_free(list);
  return found;
}

SDL_DisplayID DisplayAt(int index) {
  int n = 0;
  SDL_DisplayID* list = SDL_GetDisplays(&n);
  const SDL_DisplayID id = list && index >= 0 && index < n ? list[index] : 0;
  SDL_free(list);
  return id;
}

// Is (x, y) .. (x + w, y + h) mostly on some display (a remembered place on a monitor since unplugged
// is not used)?
bool OnSomeDisplay(int x, int y, int w, int h) {
  int n = 0;
  SDL_DisplayID* list = SDL_GetDisplays(&n);
  bool ok = false;
  for (int i = 0; list && i < n && !ok; ++i) {
    SDL_Rect r;
    if (!SDL_GetDisplayUsableBounds(list[i], &r)) continue;
    const int ix = std::max(0, std::min(x + w, r.x + r.w) - std::max(x, r.x));
    const int iy = std::max(0, std::min(y + h, r.y + r.h) - std::max(y, r.y));
    ok = ix >= std::min(w, 160) && iy >= std::min(h, 120) && y >= r.y - 8;
  }
  SDL_free(list);
  return ok;
}

// Where a new window goes: the remembered place if it is still on a display; else the remembered
// display, else another display than window 1's, else beside window 1 (right of it if it fits).
void Place(int w, int h, int& x, int& y) {
  const GraphicsSettings& g = graphics_settings();
  if (g.window2PosSet && OnSomeDisplay(g.window2X, g.window2Y, w, h)) {
    x = g.window2X, y = g.window2Y;
    return;
  }
  SDL_Window* main = aurora_get_window();
  const SDL_DisplayID mainDisplay = main ? SDL_GetDisplayForWindow(main) : SDL_GetPrimaryDisplay();
  SDL_DisplayID target = g.window2Display >= 0 ? DisplayAt(g.window2Display) : 0;
  if (!target) {
    int n = 0;
    SDL_DisplayID* list = SDL_GetDisplays(&n);
    for (int i = 0; list && i < n && !target; ++i)
      if (list[i] != mainDisplay) target = list[i];
    SDL_free(list);
  }
  SDL_Rect r;
  if (target && SDL_GetDisplayUsableBounds(target, &r)) {
    x = r.x + std::max(0, (r.w - w) / 2), y = r.y + std::max(0, (r.h - h) / 2);
    return;
  }
  int mx = 0, my = 0, mw = 0, mh = 0;
  if (main) SDL_GetWindowPosition(main, &mx, &my), SDL_GetWindowSize(main, &mw, &mh);
  if (!SDL_GetDisplayUsableBounds(mainDisplay, &r)) r = SDL_Rect{0, 0, 1920, 1080};
  x = mx + mw + 16, y = my;
  if (x + w > r.x + r.w) x = std::max(r.x, r.x + r.w - w);  // overlapping window 1 on the right, at worst
  if (y + h > r.y + r.h) y = std::max(r.y, r.y + r.h - h);
}

void RememberGeometry() {
  if (!g_window) return;
  GraphicsSettings& g = graphics_settings();
  g.fullscreen2 = g_fullscreen;
  if (!g_fullscreen && !(SDL_GetWindowFlags(g_window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED))) {
    int x = 0, y = 0, w = 0, h = 0;
    SDL_GetWindowPosition(g_window, &x, &y);
    SDL_GetWindowSize(g_window, &w, &h);
    unsigned dw = 0, dh = 0;
    settings_window_size(g, dw, dh);
    if (w > 0 && h > 0) {
      g.window2Set = static_cast<unsigned>(w) != dw || static_cast<unsigned>(h) != dh || g.window2Set;
      g.window2Width = static_cast<unsigned>(w), g.window2Height = static_cast<unsigned>(h);
    }
    g.window2X = x, g.window2Y = y, g.window2PosSet = true;
  }
  const int display = DisplayIndex(SDL_GetDisplayForWindow(g_window));
  if (display >= 0) g.window2Display = display;
}

void SaveGeometry() {
  RememberGeometry();
  const GraphicsSettings& g = graphics_settings();
  if (g.window2Set == g_saved.window2Set && g.window2Width == g_saved.window2Width && g.window2Height == g_saved.window2Height &&
      g.window2PosSet == g_saved.window2PosSet && g.window2X == g_saved.window2X && g.window2Y == g_saved.window2Y &&
      g.window2Display == g_saved.window2Display && g.fullscreen2 == g_saved.fullscreen2)
    return;
  static const char* const keys[] = {"window2", "window2_pos", "window2_display", "fullscreen2"};
  std::string message;
  if (settings_save_keys(keys, sizeof keys / sizeof keys[0], message)) g_saved = g;
  std::fprintf(stderr, "[window2] %s\n", message.c_str());
}

void ApplyFullscreen(bool on) {
  if (!g_window) return;
  if (on) SDL_SetWindowFullscreenMode(g_window, nullptr);  // borderless on the window's display
  if (!SDL_SetWindowFullscreen(g_window, on)) std::fprintf(stderr, "[window2] fullscreen: %s\n", SDL_GetError());
  g_fullscreen = on;
}

void ResizeTarget() {
  if (!g_window || !g_target) return;
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(g_window, &pw, &ph);
  if (pw > 0 && ph > 0)
    aurora::gfx::resize_present_target(g_target, static_cast<uint32_t>(pw), static_cast<uint32_t>(ph),
                                       SDL_GetWindowDisplayScale(g_window));
}

void CloseExtra(int k) {
  ExtraWindow& x = g_extra[k];
  if (x.target) aurora::gfx::destroy_present_target(x.target);
  x.target = aurora::gfx::InvalidPresentTarget;
  if (x.window) {
    SDL_DestroyWindow(x.window);
    std::fprintf(stderr, "[window%d] closed\n", k + 1);
  }
  x.window = nullptr;
  x.cropped = false;
  x.crop = -1.0f;
}

bool AnyExtraWindow() {
  for (int k = 2; k < kWindowsMax; ++k)
    if (g_extra[k].window) return true;
  return false;
}

void Close() {
  if (g_window) SaveGeometry();
  if (g_target) aurora::gfx::destroy_present_target(g_target);  // waits for its last GPU work
  g_target = aurora::gfx::InvalidPresentTarget;
  if (g_window) {
    SDL_DestroyWindow(g_window);
    std::fprintf(stderr, "[window2] closed\n");
  }
  g_window = nullptr;
  if (g_backgroundInput && !AnyExtraWindow()) aurora_set_background_input(false), g_backgroundInput = false;
}

void Open() {
  const GraphicsSettings& g = graphics_settings();
  unsigned w = g.window2Width, h = g.window2Height;
  if (!g.window2Set) settings_window_size(g, w, h);  // window 1's size
  int x = 0, y = 0;
  Place(static_cast<int>(w), static_cast<int>(h), x, y);
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, SIMMY_PORT_VERSION " - Player 2");
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, static_cast<int>(w));
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, static_cast<int>(h));
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
  // As aurora's window: WebGPU draws into it, SDL makes no graphics context of its own.
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, true);
  g_window = SDL_CreateWindowWithProperties(props);
  SDL_DestroyProperties(props);
  if (!g_window) {
    std::fprintf(stderr, "[window2] cannot create the window: %s\n", SDL_GetError());
    g_failed = true;
    return;
  }
  SDL_SetWindowMinimumSize(g_window, 320, 180);
  // A surface on aurora's device, presented by aurora's render thread like window 1's.
  const wgpu::Device device = aurora::gfx::device();
  wgpu::Instance instance = device ? device.GetAdapter().GetInstance() : wgpu::Instance{};
  wgpu::Surface surface = aurora::webgpu::create_window_surface(instance, g_window, "Player 2 window");
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(g_window, &pw, &ph);
  aurora::gfx::PresentTargetDesc desc;
  desc.label = "Player 2 window";
  desc.surface = std::move(surface);
  desc.width = static_cast<uint32_t>(std::max(1, pw));
  desc.height = static_cast<uint32_t>(std::max(1, ph));
  desc.scale = SDL_GetWindowDisplayScale(g_window);
  desc.preferredFormat = aurora::gfx::color_format();
  desc.vsync = false;  // never wait for this window's vertical blank (window 1 paces the game)
  g_target = desc.surface ? aurora::gfx::create_present_target(std::move(desc)) : aurora::gfx::InvalidPresentTarget;
  if (!g_target) {
    std::fprintf(stderr, "[window2] cannot present to the second window on this graphics API; staying with one\n");
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
    g_failed = true;
    return;
  }
  SDL_ShowWindow(g_window);
  if (g.fullscreen2) ApplyFullscreen(true);
  // Keyboard and F1 stay with window 1.
  if (SDL_Window* main = aurora_get_window()) SDL_RaiseWindow(main);
  // Both pads keep working whichever window (or another program) has the focus.
  aurora_set_background_input(true);
  g_backgroundInput = true;
  g_saved = g;
  RememberGeometry();
  g_saved.fullscreen2 = g.fullscreen2;
  std::fprintf(stderr, "[window2] opened %dx%d (%dx%d pixels) at %d,%d on display %d%s\n", static_cast<int>(w),
               static_cast<int>(h), pw, ph, x, y, DisplayIndex(SDL_GetDisplayForWindow(g_window)),
               g.fullscreen2 ? ", fullscreen" : "");
}

// PC mod: 4 players: window k + 1 for player k + 1 (k = 2, 3), window 1's size, in a 2 x 2 grid with
// windows 1 and 2 (player 3 under player 1, player 4 under player 2).
void OpenExtra(int k) {
  ExtraWindow& x = g_extra[k];
  unsigned w = 0, h = 0;
  settings_window_size(graphics_settings(), w, h);
  int mx = 0, my = 0, mw = static_cast<int>(w), mh = static_cast<int>(h);
  SDL_Window* main = aurora_get_window();
  if (main) {
    SDL_GetWindowPosition(main, &mx, &my);
    SDL_GetWindowSize(main, &mw, &mh);
    w = static_cast<unsigned>(mw), h = static_cast<unsigned>(mh);
  }
  int x0 = mx + (k & 1) * (mw + 16), y0 = my + (k / 2) * (mh + 40);
  SDL_Rect r;
  const SDL_DisplayID display = main ? SDL_GetDisplayForWindow(main) : SDL_GetPrimaryDisplay();
  if (SDL_GetDisplayUsableBounds(display, &r)) {
    if (x0 + mw > r.x + r.w) x0 = std::max(r.x, r.x + r.w - mw);
    if (y0 + mh > r.y + r.h) y0 = std::max(r.y, r.y + r.h - mh);
  }
  char title[96];
  std::snprintf(title, sizeof title, "SIMMY - Player %d", k + 1);
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x0);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y0);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, static_cast<int>(w));
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, static_cast<int>(h));
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, true);
  x.window = SDL_CreateWindowWithProperties(props);
  SDL_DestroyProperties(props);
  if (!x.window) {
    std::fprintf(stderr, "[window%d] cannot create the window: %s\n", k + 1, SDL_GetError());
    x.failed = true;
    return;
  }
  SDL_SetWindowMinimumSize(x.window, 320, 180);
  const wgpu::Device device = aurora::gfx::device();
  wgpu::Instance instance = device ? device.GetAdapter().GetInstance() : wgpu::Instance{};
  wgpu::Surface surface = aurora::webgpu::create_window_surface(instance, x.window, title);
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(x.window, &pw, &ph);
  aurora::gfx::PresentTargetDesc desc;
  desc.label = title;
  desc.surface = std::move(surface);
  desc.width = static_cast<uint32_t>(std::max(1, pw));
  desc.height = static_cast<uint32_t>(std::max(1, ph));
  desc.scale = SDL_GetWindowDisplayScale(x.window);
  desc.preferredFormat = aurora::gfx::color_format();
  desc.vsync = false;  // window 1 paces the game
  x.target = desc.surface ? aurora::gfx::create_present_target(std::move(desc)) : aurora::gfx::InvalidPresentTarget;
  if (!x.target) {
    std::fprintf(stderr, "[window%d] cannot present to another window on this graphics API\n", k + 1);
    SDL_DestroyWindow(x.window);
    x.window = nullptr;
    x.failed = true;
    return;
  }
  SDL_ShowWindow(x.window);
  if (main) SDL_RaiseWindow(main);  // keyboard and F1 stay with window 1
  if (!g_backgroundInput) aurora_set_background_input(true), g_backgroundInput = true;
  std::fprintf(stderr, "[window%d] opened %ux%u (%dx%d pixels) at %d,%d\n", k + 1, w, h, pw, ph, x0, y0);
}

bool ExtraPresentable(int k) {
  const ExtraWindow& x = g_extra[k];
  if (!x.window || !x.target) return false;
  if (SDL_GetWindowFlags(x.window) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) return false;
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(x.window, &pw, &ph);
  return pw > 0 && ph > 0;
}

bool Presentable() {
  if (!g_window || !g_target || g_hidden) return false;
  const SDL_WindowFlags flags = SDL_GetWindowFlags(g_window);
  if (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) return false;
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(g_window, &pw, &ph);
  return pw > 0 && ph > 0;
}
}  // namespace

void window2_frame() {
  if (Offscreen()) return;  // the virtual window needs nothing
  // PC mod: 4 players: windows 3 and 4 while their players are in (and split=windows).
  for (int k = 2; k < kWindowsMax; ++k) {
    const bool want = Wanted() && split_window_count_requested() > k && aurora_get_window();
    if (!want) {
      if (g_extra[k].window) CloseExtra(k);
      g_extra[k].failed = false;
      g_extra[k].closeRequested = false;
    } else if (!g_extra[k].window && !g_extra[k].failed && !g_extra[k].closeRequested) {
      OpenExtra(k);
    }
  }
  if (g_backgroundInput && !g_window && !AnyExtraWindow()) aurora_set_background_input(false), g_backgroundInput = false;
  if (!Wanted()) {
    if (g_window) Close();
    if (g_primaryCropped) {
      aurora::gfx::set_present_crop(aurora::gfx::xfb_target(), 0.0f, 0.0f, 0.0f, 0.0f);
      g_primaryCropped = false;
    }
    g_failed = false;  // another try when the mode is chosen again
    return;
  }
  if (!g_window && !g_failed && !g_hidden && aurora_get_window()) Open();
}

bool window2_handle_event(const SDL_Event& e) {
  // PC mod: 4 players: windows 3 and 4. Closing one drops its player out (the game polls
  // split_window_take_close_request); resizing resizes the target.
  for (int k = 2; k < kWindowsMax; ++k) {
    ExtraWindow& x = g_extra[k];
    if (!x.window || SDL_GetWindowFromEvent(&e) != x.window) continue;
    if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      std::fprintf(stderr, "[window%d] closed by the player: player %d drops out\n", k + 1, k + 1);
      x.closeRequested = true;
      CloseExtra(k);
    } else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || e.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
               e.type == SDL_EVENT_WINDOW_SHOWN || e.type == SDL_EVENT_WINDOW_RESTORED) {
      int pw = 0, ph = 0;
      SDL_GetWindowSizeInPixels(x.window, &pw, &ph);
      if (pw > 0 && ph > 0 && x.target)
        aurora::gfx::resize_present_target(x.target, static_cast<uint32_t>(pw), static_cast<uint32_t>(ph),
                                           SDL_GetWindowDisplayScale(x.window));
    }
    return true;
  }
  if (!g_window || SDL_GetWindowFromEvent(&e) != g_window) return false;
  switch (e.type) {
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
  case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
  case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
  case SDL_EVENT_WINDOW_RESTORED:
  case SDL_EVENT_WINDOW_SHOWN: ResizeTarget(); break;
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    std::fprintf(stderr, "[window2] closed by the player: hidden (F2 or the F1 options show it again)\n");
    window2_set_shown(false);
    break;
  case SDL_EVENT_KEY_DOWN:
    if (!e.key.repeat && (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) && (e.key.mod & SDL_KMOD_ALT))
      window2_set_fullscreen(!g_fullscreen);
    break;
  default: break;
  }
  return true;
}

void window2_set_shown(bool shown) {
  if (!Wanted()) return;
  if (!shown) {
    if (g_window) {
      SaveGeometry();
      if (g_fullscreen) ApplyFullscreen(false), graphics_settings().fullscreen2 = true;
      SDL_HideWindow(g_window);
    }
    g_hidden = true;
    return;
  }
  g_hidden = false;
  g_failed = false;
  if (!g_window) {
    Open();
  } else {
    SDL_ShowWindow(g_window);
    if (graphics_settings().fullscreen2) ApplyFullscreen(true);
    ResizeTarget();
  }
}

bool window2_shown() { return Offscreen() ? Wanted() : g_window && !g_hidden; }

void window2_set_fullscreen(bool on) {
  if (!g_window) {
    graphics_settings().fullscreen2 = on;
    return;
  }
  ApplyFullscreen(on);
  graphics_settings().fullscreen2 = on;
  ResizeTarget();
  std::fprintf(stderr, "[window2] fullscreen %s\n", on ? "on (borderless)" : "off");
}

bool window2_fullscreen() { return g_window ? g_fullscreen : graphics_settings().fullscreen2; }

void window2_shutdown() {
  for (int k = 2; k < kWindowsMax; ++k) CloseExtra(k);  // PC mod: 4 players
  Close();
}

// PC mod: 4 players.
bool split_window_open(int index) {
  if (index == 0) return true;
  if (index == 1) return window2_open();
  if (index < 2 || index >= kWindowsMax || !Wanted() || split_window_count_requested() <= index) return false;
  if (Offscreen()) return true;
  return g_extra[index].window && g_extra[index].target;
}

bool split_window_pixel_size(int index, unsigned& width, unsigned& height) {
  if (index == 1) return window2_pixel_size(width, height);
  if (index < 2 || index >= kWindowsMax || !Wanted()) return false;
  if (Offscreen()) {
    settings_window_size(graphics_settings(), width, height);  // the first window's
    return width && height;
  }
  SDL_Window* w = g_extra[index].window;
  if (!w) return false;
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(w, &pw, &ph);
  if (pw <= 0 || ph <= 0) return false;
  width = static_cast<unsigned>(pw), height = static_cast<unsigned>(ph);
  return true;
}

bool split_window_take_close_request(int index) {
  if (index < 2 || index >= kWindowsMax || !g_extra[index].closeRequested) return false;
  g_extra[index].closeRequested = false;
  return true;
}

bool window2_open() {
  if (!Wanted()) return false;
  if (Offscreen()) return true;
  return g_window && g_target && !g_hidden;
}

bool window2_pixel_size(unsigned& width, unsigned& height) {
  if (!Wanted()) return false;
  if (Offscreen()) {
    const GraphicsSettings& g = graphics_settings();
    width = g.window2Width, height = g.window2Height;
    if (!g.window2Set) settings_window_size(g, width, height);
    return width && height;
  }
  if (!g_window) return false;
  int pw = 0, ph = 0;
  SDL_GetWindowSizeInPixels(g_window, &pw, &ph);
  if (pw <= 0 || ph <= 0) return false;
  width = static_cast<unsigned>(pw), height = static_cast<unsigned>(ph);
  return true;
}

void window2_present(bool windows) {
  if (Offscreen()) return;
  const aurora::gfx::PresentTargetId primary = aurora::gfx::xfb_target();
  // PC mod: 4 players: n strips (2: the halves as before).
  const int n = windows ? split_window_count() : 2;
  static int s_cropN = 2;
  const float strip = 1.0f / static_cast<float>(n);
  // Window 1: the left half in a two-window frame, the whole frame otherwise.
  if ((windows != g_primaryCropped || (windows && n != s_cropN)) && primary) {
    if (windows) aurora::gfx::set_present_crop(primary, 0.0f, 0.0f, strip, 1.0f);
    else aurora::gfx::set_present_crop(primary, 0.0f, 0.0f, 0.0f, 0.0f);
    g_primaryCropped = windows;
  }
  // Windows 3 and 4 (PC mod: 4 players): their strip, else a mirror of window 1.
  for (int k = 2; k < kWindowsMax; ++k) {
    ExtraWindow& x = g_extra[k];
    if (!ExtraPresentable(k)) continue;
    const float u0 = windows && k < n ? static_cast<float>(k) * strip : -1.0f;
    if (u0 != x.crop) {
      if (u0 >= 0.0f) aurora::gfx::set_present_crop(x.target, u0, 0.0f, u0 + strip, 1.0f);
      else aurora::gfx::set_present_crop(x.target, 0.0f, 0.0f, 0.0f, 0.0f);
      x.crop = u0;
    }
    aurora::gfx::present_to(x.target);
  }
  const bool nChanged = windows && n != s_cropN;
  if (windows) s_cropN = n;
  if (!Presentable()) return;
  if (windows != g_cropped || nChanged) {
    if (windows) aurora::gfx::set_present_crop(g_target, strip, 0.0f, 2.0f * strip, 1.0f);
    else aurora::gfx::set_present_crop(g_target, 0.0f, 0.0f, 0.0f, 0.0f);
    g_cropped = windows;
  }
  aurora::gfx::present_to(g_target);
}

void window2_write_shots(const std::string& path, const unsigned char* rgba, unsigned width, unsigned height, int windows,
                         void (*writeBmp)(const std::string&, const unsigned char*, unsigned, unsigned)) {
  if (!Wanted() || !width || !height) return;
  const size_t dot = path.rfind('.');
  const std::string stem = dot == std::string::npos ? path : path.substr(0, dot);
  const std::string ext = dot == std::string::npos ? std::string(".bmp") : path.substr(dot);
  // PC mod: 4 players: one image per window (`windows` strips; 0: not a split frame, every window
  // mirrors, two images as before).
  const int count = windows >= 2 ? windows : std::max(2, split_window_count_requested());
  for (int win = 0; win < count; ++win) {
    unsigned ow = width, oh = height;
    float u0 = 0.0f, u1 = 1.0f;
    if (windows >= 2) {
      u0 = static_cast<float>(win) / static_cast<float>(windows), u1 = static_cast<float>(win + 1) / static_cast<float>(windows);
      if (win == 0) {
        const AuroraWindowSize s = aurora_get_window_size();
        if (s.native_fb_width && s.native_fb_height) ow = s.native_fb_width, oh = s.native_fb_height;
      } else if (!split_window_pixel_size(win, ow, oh)) {
        ow = width / static_cast<unsigned>(windows), oh = height;
      }
    }
    // Bilinear from the frame's rectangle to the window's pixels (as aurora's present resamples).
    std::vector<unsigned char> out(static_cast<size_t>(ow) * oh * 4);
    const float sx0 = u0 * static_cast<float>(width), sw = (u1 - u0) * static_cast<float>(width);
    for (unsigned y = 0; y < oh; ++y) {
      const float fy = std::clamp((static_cast<float>(y) + 0.5f) * static_cast<float>(height) / static_cast<float>(oh) - 0.5f, 0.0f,
                                  static_cast<float>(height - 1));
      const unsigned y0 = static_cast<unsigned>(fy), y1 = std::min(y0 + 1, height - 1);
      const float ty = fy - static_cast<float>(y0);
      for (unsigned x = 0; x < ow; ++x) {
        const float fx = std::clamp(sx0 + (static_cast<float>(x) + 0.5f) * sw / static_cast<float>(ow) - 0.5f, sx0,
                                    sx0 + sw - 1.0f);
        const unsigned x0 = static_cast<unsigned>(fx), x1 = std::min(x0 + 1, width - 1);
        const float tx = fx - static_cast<float>(x0);
        for (int c = 0; c < 4; ++c) {
          auto at = [&](unsigned px, unsigned py) { return static_cast<float>(rgba[(static_cast<size_t>(py) * width + px) * 4 + c]); };
          const float v = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) + (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
          out[(static_cast<size_t>(y) * ow + x) * 4 + c] = static_cast<unsigned char>(std::lround(std::clamp(v, 0.0f, 255.0f)));
        }
      }
    }
    writeBmp(stem + "_p" + std::to_string(win + 1) + ext, out.data(), ow, oh);
  }
}

}  // namespace simmy
