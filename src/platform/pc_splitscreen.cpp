// PC: co-op split screen display modes (see platform.hpp, "Co-op split screen").
//
// The game always draws in GameCube coordinates: a 640x480 frame (ENgcRenderer::InitVideo's render
// mode), and two-player live mode splits it into a left and a right half (ELiveMode::InitPlayerXWindow).
// The modes differ only in the frame aurora gives that logical 640x480 and in the aspect the 3D views
// are projected with (EGraphics::GetScreenAspect):
//   gc    the original: a 4:3 frame, each half projected at half that (2:3).
//   4x3   each half a 4:3 view, as in the GC "coop4x3" build (which drops InitPlayerXWindow's halving of
//         the aspect, 8008F354, so each half could be shown on its own 4:3 screen): here the frame is
//         8:3 (1280x480 by default), so both halves sit side by side undistorted.
//   wide  the window's aspect (16:9, 1280x720, by default): each half is half of it (8:9 at 16:9).
//   windows  each half in its own window (the second one: pc_window2.cpp), see "Two windows" below.
// Outside the split views (front end, CAS, one-player live mode) the 640x480 frame is fitted into the
// window (pillarboxed) and the aspect stays 4:3. During the split views aurora's viewport policy is
// "stretch": the logical frame covers the whole window and the projection uses the window's real aspect.
//
// PC widescreen (aspect=16:9 in simmy.ini, pc_settings.cpp): outside the split views the frame is
// stretched over the window as well and the 3D views use its aspect (more world at the sides, the same
// vertical field of view). What is drawn in 640x480 screen coordinates is put back into the 4:3 centre
// by display_map_viewport (ENgcRenderStateCache::Apply), so 2D is never stretched.
#include "simmy/platform.hpp"
#include "pc_settings.hpp"

#include <aurora/aurora.h>
#include <dolphin/gx/GXAurora.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace simmy {

bool g_displayMap2D = false;

namespace {
bool g_stretched = false;
float g_aspect = 4.0f / 3.0f;
bool g_drawnThisFrame = false;  // split_screen_frame was called this frame (live mode drew)
}  // namespace

// The mode is a graphics setting (split=, --split, SIMMY_SPLIT: pc_settings.cpp).
bool set_split_mode(const char* name) {
  const int before = graphics_settings().split;
  std::string value = name;
  if (value != "gc" && value != "4x3" && value != "wide" && value != "windows") {
    std::fprintf(stderr, "[split] unknown split mode \"%s\" (gc, 4x3, wide, windows)\n", name);
    return false;
  }
  settings_set_value("split", value);
  return graphics_settings().split != before || value == settings_split_name(before);
}

SplitMode split_mode() {
  const int s = graphics_settings().split;
  return s == 1 ? SplitMode::FourByThree : s == 2 ? SplitMode::Wide : s == 3 ? SplitMode::Windows : SplitMode::Gc;
}

void split_default_frame_size(unsigned& width, unsigned& height) {
  switch (split_mode()) {
  case SplitMode::Gc: width = 640, height = 480; break;
  case SplitMode::FourByThree: width = 1280, height = 480; break;
  case SplitMode::Wide: width = 1280, height = 720; break;
  case SplitMode::Windows: width = 1280, height = 720; break;  // each window
  }
}

bool dev_two_player() {
  static const int on = [] {
    const char* s = std::getenv("SIMMY_TWO_PLAYER");
    return s && *s && *s != '0' ? 1 : 0;
  }();
  return on != 0;
}

// Widescreen state: the share of the frame's width (kx) and height (ky) the 4:3 centre covers, and the
// HUD anchor of what is being drawn (pc_hud.cpp).
namespace {
float g_kx = 1.0f, g_ky = 1.0f;
float g_hudShift = 0.0f;
int g_hudMode = 0;  // display_set_hud_anchor

// Two windows (split=windows). The game draws its two halves into one 640x480 logical frame as in the
// other modes; the EFB is made twice the larger window's width (AuroraSetRenderSizeOverride, from the
// next frame on) and its left half is presented to window 1, its right half to window 2 (aurora's
// set_present_crop, pc_window2.cpp). Each half is projected at its own window's aspect
// (split_screen_aspect while InitPlayerXWindow draws that player), and everything 2D is mapped into
// its window's half undistorted: the GC's per-player HUD (laid out in its 320x480 half of the stage)
// at the height of the window, centred or at the edges (hud=), and a dialog over the whole stage (the
// pause menu) once into each window, at the 4:3 size it has in one-player widescreen.
bool g_winSplit = false;  // this frame's halves go to two windows
int g_drawPlayer = 0;     // the half InitPlayerXWindow selected last
float g_winAspect[2] = {16.0f / 9.0f, 16.0f / 9.0f};
int g_hudWindow = -1;     // display_set_hud_window
bool g_hudWhole = false;
int g_hudAnchor = 0;
unsigned g_overrideW = 0, g_overrideH = 0;  // AuroraSetRenderSizeOverride in effect

// Co-op private screens (coop_set_private_screen): the Apt root instances one player has up on their
// own, and whether this frame is a one-window split frame with one up (split_half_frame).
struct PrivateScreen {
  char name[8];
  int player;
};
PrivateScreen g_private[4] = {};
int g_privateCount = 0;
bool g_splitHalf = false;

// Window 1's size in pixels (the headless surface in offscreen runs).
void Window1Size(unsigned& w, unsigned& h) {
  const AuroraWindowSize size = aurora_get_window_size();
  w = size.native_fb_width, h = size.native_fb_height;
  if (!w || !h) split_default_frame_size(w, h);
  if ((!w || !h) || (!size.native_fb_width && graphics_settings().windowSet))
    w = graphics_settings().windowWidth, h = graphics_settings().windowHeight;
}

// SIMMY_WIN2_TRACE=<frame> (or d<dev frame>): every 2D mapping of that frame (split=windows).
bool Trace() {
  static const bool any = std::getenv("SIMMY_WIN2_TRACE") != nullptr;
  return any && dev_trace_frame("SIMMY_WIN2_TRACE");
}

// The frame's aspect while it is stretched over the window.
float StretchedAspect() {
  unsigned w = 0, h = 0;
  split_default_frame_size(w, h);
  if (graphics_settings().windowSet) w = graphics_settings().windowWidth, h = graphics_settings().windowHeight;
  else if (graphics_settings().wide && split_mode() == SplitMode::Gc)
    w = 1280, h = static_cast<unsigned>(std::lround(1280.0 / graphics_settings().defaultAspect));
  u32 rw = 0, rh = 0;
  AuroraGetRenderSize(&rw, &rh);
  // The policy is applied when aurora's next frame starts; until then the configured size stands in.
  const AuroraWindowSize size = aurora_get_window_size();
  if (size.native_fb_width && size.native_fb_height) rw = size.native_fb_width, rh = size.native_fb_height;
  if (rw && rh) w = rw, h = rh;
  return static_cast<float>(w) / static_cast<float>(h);
}

// One frame's display mode: the split views as before; otherwise widescreen if configured.
void UpdateDisplay(bool split) {
  // Two windows: the halves fill the frame (stretch), each projected at its window's aspect.
  g_winSplit = split && split_mode() == SplitMode::Windows && !platform_headless() && window2_open();
  if (g_winSplit) {
    if (!g_stretched) {
      AuroraSetViewportPolicy(AURORA_VIEWPORT_STRETCH);
      g_stretched = true;
    }
    unsigned w = 0, h = 0;
    Window1Size(w, h);
    g_winAspect[0] = static_cast<float>(w) / static_cast<float>(h);
    if (window2_pixel_size(w, h)) g_winAspect[1] = static_cast<float>(w) / static_cast<float>(h);
    g_aspect = 2.0f * g_winAspect[0];
    g_displayMap2D = true;
    return;
  }
  const bool wide = !split && graphics_settings().wide;
  const bool stretch = (split && split_mode() != SplitMode::Gc) || wide;
  if (stretch != g_stretched && !platform_headless()) {
    AuroraSetViewportPolicy(stretch ? AURORA_VIEWPORT_STRETCH : AURORA_VIEWPORT_FIT);
    g_stretched = stretch;
  }
  g_aspect = 4.0f / 3.0f;
  // One window, both halves: only a co-op private screen's 2D is mapped (into its player's half).
  g_splitHalf = split && g_privateCount > 0;
  g_displayMap2D = g_splitHalf;
  if (!g_stretched) return;
  g_aspect = StretchedAspect();
  if (g_splitHalf) return;
  if (!wide) return;
  const float gc = 4.0f / 3.0f;
  g_kx = std::min(1.0f, gc / g_aspect);
  g_ky = std::min(1.0f, g_aspect / gc);
  g_displayMap2D = g_kx < 0.999f || g_ky < 0.999f;
}
}  // namespace

void split_screen_frame(bool split) {
  g_drawnThisFrame = true;
  UpdateDisplay(split);
}

void split_screen_player(int player) { g_drawPlayer = player == 1 ? 1 : 0; }

bool split_draws_divider() { return !g_winSplit; }

bool split_windows_frame() { return g_winSplit && g_drawnThisFrame; }

bool split_half_frame() { return g_splitHalf && !g_winSplit && g_drawnThisFrame; }

void coop_set_private_screen(const char* instance, int player) {
  if (!instance) return;
  int i = 0;
  while (i < g_privateCount && std::strncmp(g_private[i].name, instance, sizeof g_private[i].name) != 0) ++i;
  if (player < 0) {
    if (i == g_privateCount) return;
    g_private[i] = g_private[--g_privateCount];
  } else {
    if (i == g_privateCount) {
      if (g_privateCount == 4) return;
      ++g_privateCount;
      std::snprintf(g_private[i].name, sizeof g_private[i].name, "%s", instance);
    }
    g_private[i].player = player == 1 ? 1 : 0;
  }
  std::printf("[coop] %s is %s\n", instance,
              player < 0 ? "no longer private" : player == 1 ? "private to player 2" : "private to player 1");
  // A split frame already set up this frame maps the screen from the next one on.
}

int coop_private_player() { return g_privateCount > 0 ? g_private[0].player : -1; }

int coop_private_screen_owner(const char* instance) {
  if (!instance) return -1;
  for (int i = 0; i < g_privateCount; ++i)
    if (std::strncmp(g_private[i].name, instance, sizeof g_private[i].name) == 0) return g_private[i].player;
  return -1;
}

void display_set_hud_window(int window, bool wholeStage, int anchor) {
  g_hudWindow = window;
  g_hudWhole = wholeStage;
  g_hudAnchor = anchor;
}

void display_settings_changed() { UpdateDisplay(false); }

float display_side_bar() { return 320.0f * (1.0f - g_kx); }

bool display_hud_edges() {
  if (g_winSplit) return graphics_settings().hud == HudMode::Edges;
  return g_displayMap2D && graphics_settings().hud == HudMode::Edges && g_kx < 0.999f;
}

void display_set_hud_anchor(float shiftX, int mode) {
  g_hudShift = shiftX;
  g_hudMode = mode;
}

// ENgcRenderStateCache::Apply, widescreen only: a draw that is not a perspective view made for the
// stretched frame goes into the 4:3 centre (moved by the HUD anchor), viewport and scissor alike.
// A "bleed" draw (HUD mode 2) keeps that placement but is not clipped to the centre: the viewport and
// scissor span the frame's width and the placement moves into the projection (clip x' = a x + b w).
namespace {
// The aspect a half of the frame is shown at: its window's (split=windows), else half the frame's.
float HalfAspect(int half) { return g_winSplit ? g_winAspect[half] : 0.5f * g_aspect; }

// One window's share of the frame for a 2D draw (split=windows): its half [r0, r0 + 320] of the 640
// logical units, holding content laid out over the half stage (320 wide, from x = r0) or the whole stage
// (640, from 0), fitted undistorted into the window's aspect and placed by the anchor. A one-window
// split frame maps a co-op private screen's draws the same way into its player's half of the frame.
bool MapWindows(float vp[6], int sc[4], const float (*proj)[4], float outProj[4][4]) {
  const bool perspective = proj[3][2] == -1.0f;
  const float cx = vp[0] + vp[2] * 0.5f;
  const int half = cx < 320.0f ? 0 : 1;
  if (perspective && proj[0][0] != 0.0f && vp[2] > 0.0f && vp[3] > 0.0f) {
    // A player's view, projected for its window (one logical unit across is 1.5 x aspect units).
    const float projAspect = proj[1][1] / proj[0][0];
    const float viewAspect = HalfAspect(half) * 1.5f * vp[2] / vp[3];
    if (std::fabs(projAspect / viewAspect - 1.0f) < 0.01f) return false;
  }
  int window = half;
  bool whole = false;
  if (g_hudWindow >= 0) {
    window = g_hudWindow, whole = g_hudWhole;
  } else if (vp[0] < 319.0f && vp[0] + vp[2] > 321.0f) {
    // Not Apt, over both halves: the game's own full-screen 2D, into the window of the player whose
    // view was drawn last.
    window = g_drawPlayer, whole = true;
  }
  const float r0 = 320.0f * static_cast<float>(window), rw = 320.0f;
  const float cw = whole ? 640.0f : 320.0f, c0 = whole ? 0.0f : r0;
  const float aspect = HalfAspect(window);
  const float fit = std::min(1.0f, aspect * 480.0f / cw);  // content scale (1 = the window's height)
  const float k = fit * rw / (aspect * 480.0f);             // logical units per content unit, across
  const float ky = fit, y0 = 240.0f * (1.0f - fit);
  const float bar = 0.5f * (rw - cw * k);
  float kx = k, x0 = r0 + bar - c0 * k;
  switch (g_hudAnchor) {
  case 1: kx = rw / cw, x0 = r0 - c0 * kx; break;  // stretched over the window
  case 3: x0 = r0 - c0 * k; break;                 // left edge
  case 4: x0 = r0 + 2.0f * bar - c0 * k; break;    // right edge
  default: break;
  }
  const float vx = x0 + vp[0] * kx, vw = vp[2] * kx;
  if (Trace())
    std::fprintf(stderr, "[win2] vp %.0f,%.0f %.0fx%.0f sc %d,%d %dx%d %s -> window %d %s anchor %d: x %.1f..%.1f\n", vp[0], vp[1],
                 vp[2], vp[3], sc[0], sc[1], sc[2], sc[3], perspective ? "persp" : "ortho", window, whole ? "whole" : "half",
                 g_hudAnchor, vx, vx + vw);
  vp[1] = y0 + vp[1] * ky;
  vp[3] *= ky;
  const float t = y0 + static_cast<float>(sc[1]) * ky, b = y0 + static_cast<float>(sc[1] + sc[3]) * ky;
  const int it = std::clamp(static_cast<int>(std::floor(t)), 0, 480), ib = std::clamp(static_cast<int>(std::ceil(b)), it, 480);
  sc[1] = it, sc[3] = ib - it;
  const int lo = static_cast<int>(r0), hi = static_cast<int>(r0 + rw);
  if (g_hudAnchor == 2 && vw > 0.0f) {
    // Bleed: placed as centred, but the viewport and scissor cover the window's whole half.
    const float a = vw / rw, off = (2.0f * (vx - r0) + vw) / rw - 1.0f;
    for (int c = 0; c < 4; ++c) {
      for (int r = 0; r < 4; ++r) outProj[r][c] = proj[r][c];
      outProj[0][c] = a * proj[0][c] + off * proj[3][c];
    }
    vp[0] = r0, vp[2] = rw;
    sc[0] = lo, sc[2] = hi - lo;
    return true;
  }
  vp[0] = vx;
  vp[2] = vw;
  const float l = x0 + static_cast<float>(sc[0]) * kx, r = x0 + static_cast<float>(sc[0] + sc[2]) * kx;
  const int il = std::clamp(static_cast<int>(std::floor(l)), lo, hi), ir = std::clamp(static_cast<int>(std::ceil(r)), il, hi);
  sc[0] = il, sc[2] = ir - il;
  return false;
}
}  // namespace

float display_window_extend(float stageWidth) {
  if (!g_winSplit || g_hudWindow < 0) return 0.0f;
  const float cw = g_hudWhole ? 640.0f : 320.0f, aspect = g_winAspect[g_hudWindow];
  const float fit = std::min(1.0f, aspect * 480.0f / cw), k = fit * 320.0f / (aspect * 480.0f);
  const float bar = 0.5f * (320.0f - cw * k);
  return bar > 0.0f ? bar / k * stageWidth / 640.0f : 0.0f;
}

bool display_map_viewport(float vp[6], int sc[4], const float (*proj)[4], float outProj[4][4]) {
  if (g_winSplit) return MapWindows(vp, sc, proj, outProj);
  if (g_splitHalf) return g_hudWindow >= 0 ? MapWindows(vp, sc, proj, outProj) : false;
  const bool perspective = proj[3][2] == -1.0f;
  if (perspective && proj[0][0] != 0.0f && vp[2] > 0.0f && vp[3] > 0.0f) {
    const float projAspect = proj[1][1] / proj[0][0];
    const float frameAspect = g_aspect * (vp[2] / 640.0f) / (vp[3] / 480.0f);
    if (std::fabs(projAspect / frameAspect - 1.0f) < 0.01f) return false;  // a 3D view at the wide aspect
  }
  float kx = g_kx, x0 = 320.0f * (1.0f - g_kx) + g_hudShift;
  if (g_hudMode == 1) kx = 1.0f, x0 = 0.0f;
  const float ky = g_ky, y0 = 240.0f * (1.0f - g_ky);
  const float vx = x0 + vp[0] * kx, vw = vp[2] * kx;
  vp[1] = y0 + vp[1] * ky;
  vp[3] *= ky;
  const float t = y0 + static_cast<float>(sc[1]) * ky, b = y0 + static_cast<float>(sc[1] + sc[3]) * ky;
  const int it = std::clamp(static_cast<int>(std::floor(t)), 0, 480), ib = std::clamp(static_cast<int>(std::ceil(b)), it, 480);
  sc[1] = it, sc[3] = ib - it;
  if (g_hudMode == 2 && vw > 0.0f) {
    const float a = vw / 640.0f, off = (2.0f * vx + vw) / 640.0f - 1.0f;
    for (int c = 0; c < 4; ++c) {
      for (int r = 0; r < 4; ++r) outProj[r][c] = proj[r][c];
      outProj[0][c] = a * proj[0][c] + off * proj[3][c];
    }
    vp[0] = 0.0f, vp[2] = 640.0f;
    sc[0] = 0, sc[2] = 640;
    return true;
  }
  vp[0] = vx;
  vp[2] = vw;
  const float l = x0 + static_cast<float>(sc[0]) * kx, r = x0 + static_cast<float>(sc[0] + sc[2]) * kx;
  const int il = std::clamp(static_cast<int>(std::floor(l)), 0, 640), ir = std::clamp(static_cast<int>(std::ceil(r)), il, 640);
  sc[0] = il, sc[2] = ir - il;
  return false;
}

float split_screen_aspect() { return g_winSplit ? 2.0f * g_winAspect[g_drawPlayer] : g_aspect; }

// End of each game frame: a frame live mode did not draw (front end, loading) goes back to 4:3, fitted.
// PC widescreen: also each frame, so the aspect follows the window when it is resized.
void split_screen_end_frame() {
  if (!g_drawnThisFrame && (g_stretched || graphics_settings().wide || g_winSplit || g_splitHalf)) UpdateDisplay(false);
  g_drawnThisFrame = false;
  // split=windows: the next frame's EFB holds both windows' halves at their resolution: twice the
  // larger window's width (the halves split at 320 logical units), its height; the internal
  // resolution setting scales that. Otherwise aurora sizes the EFB as before.
  unsigned ow = 0, oh = 0;
  if (g_winSplit && !platform_headless()) {
    unsigned w1 = 0, h1 = 0, w2 = 0, h2 = 0;
    Window1Size(w1, h1);
    if (!window2_pixel_size(w2, h2)) w2 = w1, h2 = h1;
    double rw = std::max(w1, w2), rh = std::max(h1, h2);
    const GraphicsSettings& g = graphics_settings();
    const double scale = g.renderScale > 0.0f ? g.renderScale : g.renderHeight ? g.renderHeight / 480.0 : 0.0;
    if (scale > 0.0) rw *= 480.0 * scale / rh, rh = 480.0 * scale;
    const double limit = 8192.0;  // keep the EFB within every backend's texture size
    if (2.0 * rw > limit) rh *= limit / (2.0 * rw), rw = limit / 2.0;
    if (rh > limit) rw *= limit / rh, rh = limit;
    ow = 2 * static_cast<unsigned>(std::max(1.0, std::round(rw))), oh = static_cast<unsigned>(std::max(1.0, std::round(rh)));
  }
  if (ow != g_overrideW || oh != g_overrideH) {
    g_overrideW = ow, g_overrideH = oh;
    AuroraSetRenderSizeOverride(ow, oh);
    std::fprintf(stderr, "[split] windows: EFB %s %ux%u\n", ow ? "fixed at" : "automatic again", ow, oh);
  }
}

}  // namespace simmy
