// PC process entry and main-thread loop.
//
//   simmy [--iso <disc image>] [--headless] [--data <files dir>] [--fs-test] [game args...]
//   simmy --version | --mathcheck [file]
//
// The game's files come from the GameCube disc image (--iso, else iso= in simmy.ini, which
// SimmyLauncher writes); --data <dir> (an extracted disc, for development and tests) wins over it.
//
// Startup mirrors the GameCube up to the app thread: main() runs MainInit, then StartMainThread.
// On the GameCube that started the app thread and parked the main thread in EEngine::Idle. On PC the
// app's Main() runs on the main thread instead (ENgcEngine::Idle), because the window, the GPU and
// input must stay on one thread; the game's GX calls already happen on its app thread. Window events
// are pumped once per game frame (platform_frame_tick).
//
// --headless skips the window and GPU so the boot path can run without a display.
// Graphics settings (simmy.ini, --window, --resolution, --aspect, ...) are read by pc_settings.cpp.
#if defined(__SANITIZE_ADDRESS__) && !defined(_MSC_VER)
#include <sanitizer/lsan_interface.h>
#endif
#include "simmy/version.hpp"
#include "simmy/detmath.hpp"
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"
#include "pc_settings.hpp"
#include "pc_kbm.hpp"
#include "pc_overlay.hpp"
#include "pc_net.hpp"
#include "pc_window2.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.hpp>
#include <aurora/main.h>
#include <aurora/pad.hpp>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <dolphin/gx/GXAurora.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

int simmy_game_main(int argc, char** argv);
extern "C" EFile* efopen(char* name, char* mode);
extern "C" int efread(void* dst, int size, int count, EFile* f);
extern "C" int efclose(EFile* f);
// aurora's GX command buffer (lib/gx/fifo.cpp; not in its public headers).
namespace aurora::gx::fifo {
uint32_t get_buffer_size();
void clear_buffer();
}  // namespace aurora::gx::fifo

namespace simmy {
unsigned stub_unique_count();

static bool g_headless = false;
static void (*g_onQuit)() = nullptr;
static bool g_quitRequested = false;
static bool g_gameFramePresented = false;
static std::chrono::steady_clock::time_point g_lastTick = std::chrono::steady_clock::now();

bool platform_headless() { return g_headless; }
void platform_set_quit_handler(void (*onQuitRequested)()) { g_onQuit = onQuitRequested; }

// The options overlay's Quit: the same path as closing the window (the game winds down on its own).
void platform_request_quit() {
  net_local_quit();  // PC mod: network play: the partner is told (pc_net.cpp)
  if (g_quitRequested) return;
  g_quitRequested = true;
  std::fprintf(stderr, "[simmy] quit requested\n");
  if (g_onQuit) g_onQuit();
}

bool platform_pump_events() {
  if (!g_headless) {
    // Dev/test: SIMMY_VIRTUAL_PADS=N attaches N virtual SDL gamepads, as if N controllers were plugged
    // in, to check that each reaches its own port. aurora starts SDL's controller support on its first
    // update, so they are attached on the second.
    static int s_pumps = 0;
    if (++s_pumps == 2) {
      if (const char* v = std::getenv("SIMMY_VIRTUAL_PADS")) {
        const int n = std::atoi(v);
        // Offscreen aurora never starts SDL's controller support; start it here for the test.
        SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD);
        for (int i = 0; i < n && i < 4; ++i) {
          SDL_VirtualJoystickDesc desc;
          SDL_INIT_INTERFACE(&desc);
          desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
          desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
          desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
          desc.name = "SIMMY virtual pad";
          if (!SDL_AttachVirtualJoystick(&desc)) std::fprintf(stderr, "[pad] virtual pad: %s\n", SDL_GetError());
        }
      }
    }
    for (const AuroraEvent* e = aurora_update(); e && e->type != AURORA_NONE; ++e) {
      if (e->type == AURORA_EXIT && !g_quitRequested) {
        g_quitRequested = true;
        std::fprintf(stderr, "[simmy] quit requested by the window (closed, Alt+F4 or a quit event)\n");
        net_local_quit();  // PC mod: network play
        if (g_onQuit) g_onQuit();
      }
      // The second player's window (split=windows) handles its own events (pc_window2.cpp).
      if (e->type == AURORA_SDL_EVENT && window2_handle_event(e->sdl)) continue;
      if (e->type == AURORA_SDL_EVENT) overlay_handle_event(e->sdl);  // F1: options overlay (pc_overlay.cpp)
      if (e->type == AURORA_SDL_EVENT) kbm_handle_event(e->sdl);      // player 1's keyboard and mouse (pc_kbm.cpp)
      // Alt+Enter: windowed <-> fullscreen (pc_settings.cpp).
      if (e->type == AURORA_SDL_EVENT && e->sdl.type == SDL_EVENT_KEY_DOWN && !e->sdl.key.repeat &&
          (e->sdl.key.key == SDLK_RETURN || e->sdl.key.key == SDLK_KP_ENTER) && (e->sdl.key.mod & SDL_KMOD_ALT))
        settings_toggle_fullscreen();
      // F2: show / hide the second player's window (split=windows).
      if (e->type == AURORA_SDL_EVENT && e->sdl.type == SDL_EVENT_KEY_DOWN && !e->sdl.key.repeat &&
          e->sdl.key.key == SDLK_F2 && split_mode() == SplitMode::Windows)
        window2_set_shown(!window2_shown());
    }
    // F1 also from the keyboard state (pc_overlay.cpp), while window 1 has the keyboard.
    if (split_mode() != SplitMode::Windows || SDL_GetKeyboardFocus() == aurora_get_window()) overlay_poll_keys();
    window2_frame();
  }
  return !g_quitRequested;
}

DevOptions& dev_options() {
  static DevOptions options;
  return options;
}

namespace {
unsigned g_frameNumber = 0;  // frames presented so far
AuroraReadbackStream g_shotStream = 0;
u32 g_shotWidth = 0, g_shotHeight = 0;
bool g_shotPending = false;
int g_shotWindows = 0;  // the shot's frame had the n-window layout (split=windows; PC mod: 4 players), 0 if not

void write_bmp(const std::string& path, const unsigned char* rgba, u32 w, u32 h) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "[simmy] cannot write %s\n", path.c_str());
    return;
  }
  const u32 row = (w * 3 + 3) & ~3u, size = 54 + row * h;
  unsigned char hdr[54] = {'B', 'M'};
  auto put32 = [&](int at, u32 v) {
    for (int i = 0; i < 4; ++i) hdr[at + i] = static_cast<unsigned char>(v >> (8 * i));
  };
  put32(2, size);
  put32(10, 54);
  put32(14, 40);
  put32(18, w);
  put32(22, h);
  hdr[26] = 1;
  hdr[28] = 24;
  put32(34, row * h);
  std::fwrite(hdr, 1, sizeof hdr, f);
  std::vector<unsigned char> line(row, 0);
  for (u32 y = 0; y < h; ++y) {
    const unsigned char* src = rgba + static_cast<size_t>(h - 1 - y) * w * 4;  // BMP rows are bottom-up
    for (u32 x = 0; x < w; ++x) {
      line[x * 3 + 0] = src[x * 4 + 2];
      line[x * 3 + 1] = src[x * 4 + 1];
      line[x * 3 + 2] = src[x * 4 + 0];
    }
    std::fwrite(line.data(), 1, row, f);
  }
  std::fclose(f);
  std::fprintf(stderr, "[simmy] saved %ux%u screenshot to %s\n", w, h, path.c_str());
}

// Collects a requested screenshot once the GPU has delivered it (a frame or two later).
std::string g_shotPath;  // where the pending capture goes

// With MSAA aurora turns its readback off, so screenshots then take the resolved (single-sample)
// colour through aurora's public pass API: resolve_pass, an encoder task copying it into a buffer,
// and a map once the frame is submitted.
struct MsaaShot {
  wgpu::Texture texture;
  wgpu::Buffer buffer;
  wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
  u32 width = 0, height = 0, pitch = 0;
  std::atomic<int> state{0};  // 0 waiting, 1 mapped, 2 failed
  std::string path;
};
MsaaShot* g_msaaShot = nullptr;
aurora::gfx::EncoderTaskId g_msaaShotTask = 0;

void MsaaShotEncode(const aurora::gfx::EncoderTaskContext& ctx, const wgpu::CommandEncoder& cmd, const void* payload,
                    size_t, void*) {
  MsaaShot* shot = nullptr;
  std::memcpy(&shot, payload, sizeof shot);
  wgpu::BufferDescriptor desc{};
  desc.size = static_cast<uint64_t>(shot->pitch) * shot->height;
  desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  shot->buffer = ctx.device.CreateBuffer(&desc);
  wgpu::TexelCopyTextureInfo source{};
  source.texture = shot->texture;
  wgpu::TexelCopyBufferInfo destination{};
  destination.layout.bytesPerRow = shot->pitch;
  destination.layout.rowsPerImage = shot->height;
  destination.buffer = shot->buffer;
  const wgpu::Extent3D size{shot->width, shot->height, 1};
  cmd.CopyTextureToBuffer(&source, &destination, &size);
}

void MsaaShotSubmitted(const aurora::gfx::EncoderTaskCompletionContext&, const void* payload, size_t, void*) {
  MsaaShot* shot = nullptr;
  std::memcpy(&shot, payload, sizeof shot);
  shot->buffer.MapAsync(wgpu::MapMode::Read, 0, static_cast<uint64_t>(shot->pitch) * shot->height,
                        wgpu::CallbackMode::AllowSpontaneous,
                        [shot](wgpu::MapAsyncStatus status, wgpu::StringView) {
                          shot->state = status == wgpu::MapAsyncStatus::Success ? 1 : 2;
                        });
}

bool CaptureMsaaShot(const std::string& path) {
  if (g_msaaShot) return false;
  if (!g_msaaShotTask) {
    aurora::gfx::EncoderTaskDescriptor desc;
    desc.label = "simmy screenshot";
    desc.callback = &MsaaShotEncode;
    desc.afterSubmit = &MsaaShotSubmitted;
    g_msaaShotTask = aurora::gfx::register_encoder_task_type(desc);
  }
  aurora::gfx::ResolveDesc want;
  aurora::gfx::ResolvedTargets out;
  if (!g_msaaShotTask || !aurora::gfx::resolve_pass(want, out) || !out.colorTexture) return false;
  auto* shot = new MsaaShot;
  shot->texture = out.colorTexture;
  shot->format = out.colorFormat;
  shot->width = out.width;
  shot->height = out.height;
  shot->pitch = (out.width * 4 + 255) & ~255u;
  shot->path = path;
  if (!aurora::gfx::push_encoder_task(g_msaaShotTask, &shot, sizeof shot)) {
    delete shot;
    return false;
  }
  g_msaaShot = shot;
  return true;
}

void PollMsaaShot() {
  MsaaShot* shot = g_msaaShot;
  if (!shot || shot->state == 0) return;
  if (shot->state == 1) {
    const auto* src = static_cast<const unsigned char*>(
        shot->buffer.GetConstMappedRange(0, static_cast<size_t>(shot->pitch) * shot->height));
    std::vector<unsigned char> rgba(static_cast<size_t>(shot->width) * shot->height * 4);
    const bool bgra = shot->format == wgpu::TextureFormat::BGRA8Unorm || shot->format == wgpu::TextureFormat::BGRA8UnormSrgb;
    for (u32 y = 0; src && y < shot->height; ++y)
      for (u32 x = 0; x < shot->width; ++x) {
        const unsigned char* p = src + static_cast<size_t>(y) * shot->pitch + x * 4;
        unsigned char* q = &rgba[(static_cast<size_t>(y) * shot->width + x) * 4];
        q[0] = bgra ? p[2] : p[0], q[1] = p[1], q[2] = bgra ? p[0] : p[2], q[3] = p[3];
      }
    shot->buffer.Unmap();
    overlay_composite_shot(rgba.data(), shot->width, shot->height);
    kbm_composite_shot(rgba.data(), shot->width, shot->height);
    if (src) write_bmp(shot->path, rgba.data(), shot->width, shot->height);
    if (src) window2_write_shots(shot->path, rgba.data(), shot->width, shot->height, g_shotWindows, &write_bmp);
  } else {
    std::fprintf(stderr, "[simmy] screenshot readback failed\n");
  }
  delete shot;
  g_msaaShot = nullptr;
  g_shotPending = false;
}


void poll_screenshot() {
  if (!g_shotPending) return;
  if (graphics_settings().msaa > 1) return PollMsaaShot();
  std::vector<unsigned char> pixels(static_cast<size_t>(g_shotWidth) * g_shotHeight * 4);
  AuroraReadbackDest dest{1, pixels.data(), static_cast<u32>(pixels.size()), GX_FALSE};
  if (AuroraReadbackGet(g_shotStream, &dest, 1) && dest.found) {
    overlay_composite_shot(pixels.data(), g_shotWidth, g_shotHeight);
    kbm_composite_shot(pixels.data(), g_shotWidth, g_shotHeight);
    write_bmp(g_shotPath, pixels.data(), g_shotWidth, g_shotHeight);
    window2_write_shots(g_shotPath, pixels.data(), g_shotWidth, g_shotHeight, g_shotWindows, &write_bmp);
    g_shotPending = false;
  }
}

// PC development aid: SIMMY_FRAMES_FROM_HOUSE=1 counts --screenshot, --frames, SIMMY_SHOTS and
// SIMMY_GFX_LOWRES_UNTIL from the first presented frame with a house loaded (so test shots do not
// depend on how long the front end took). Before that no shot is taken and the run does not end.
unsigned g_housePresented = 0, g_houseTick = 0;  // the counters' values at the house (0: not yet)
bool FramesFromHouse() {
  static const bool on = [] {
    const char* e = std::getenv("SIMMY_FRAMES_FROM_HOUSE");
    return e && *e && *e != '0';
  }();
  return on;
}
// `frame` relative to `base` (1 = the house's frame); 0 while waiting for the house.
unsigned FromHouse(unsigned frame, unsigned base) {
  if (!FramesFromHouse()) return frame;
  return base ? frame - base + 1 : 0;
}

// PC development aid: SIMMY_SHOTS=<frame>,<frame>,... also saves those frames next to the
// --screenshot file (its name with _<frame> before the extension).
bool ExtraShotFrame(unsigned frame) {
  static std::vector<unsigned> frames = [] {
    std::vector<unsigned> out;
    if (const char* env = std::getenv("SIMMY_SHOTS"))
      for (const char* p = env; *p;) {
        out.push_back(static_cast<unsigned>(std::strtoul(p, const_cast<char**>(&p), 10)));
        while (*p == ',' || *p == ' ') ++p;
      }
    return out;
  }();
  return std::find(frames.begin(), frames.end(), frame) != frames.end();
}
}  // namespace

// PC development aid: SIMMY_FRAME_STATS=1 prints every 300 presented frames the average time the game
// spends between the frame's start and its hand-over to aurora (update + draw recording, the GX
// commands), aurora's end of frame, and the GX command bytes a frame.
namespace {
struct FrameStats {
  bool on = std::getenv("SIMMY_FRAME_STATS") != nullptr;
  std::chrono::steady_clock::time_point begin;
  double game = 0, end = 0, bytes = 0;
  unsigned frames = 0;
};
FrameStats& Stats() {
  static FrameStats s;
  return s;
}
}  // namespace

unsigned platform_dev_frame() { return FromHouse(g_frameNumber + 1, g_housePresented); }

bool dev_trace_frame(const char* variable) {
  const char* v = std::getenv(variable);
  if (!v || !*v) return false;
  if (*v == 'd') return platform_dev_frame() == static_cast<unsigned>(std::strtoul(v + 1, nullptr, 10));
  return _framecount == std::atoi(v);
}

bool platform_begin_frame() {
  if (g_headless || !aurora_begin_frame()) return false;
  if (Stats().on) Stats().begin = std::chrono::steady_clock::now();
  // Offscreen runs are for tests and screenshots: wait for shader pipelines instead of skipping draws.
  if (dev_options().offscreen) AuroraSetDrawsBlocking(true);
  return true;
}
void platform_end_frame() {
  ++g_frameNumber;
  const DevOptions& dev = dev_options();
  const unsigned devFrame = FromHouse(g_frameNumber, g_housePresented);
  overlay_frame(devFrame);  // the options overlay's ImGui window, inside aurora's ImGui frame
  const bool extra = !dev.screenshotPath.empty() && !g_shotPending && devFrame && ExtraShotFrame(devFrame);
  if ((dev.screenshotFrame && devFrame == dev.screenshotFrame) || extra) {
    if (!g_shotStream) g_shotStream = AuroraReadbackCreateStream();
    g_shotPath = dev.screenshotPath;
    if (extra) {
      const size_t dot = g_shotPath.rfind('.');
      g_shotPath.insert(dot == std::string::npos ? g_shotPath.size() : dot, "_" + std::to_string(devFrame));
    }
    g_shotPending = graphics_settings().msaa > 1 ? CaptureMsaaShot(g_shotPath)
                                                 : AuroraReadbackCaptureColor(g_shotStream, 1, &g_shotWidth, &g_shotHeight);
    g_shotWindows = split_windows_frame() ? split_window_count() : 0;
    if (!g_shotPending) std::fprintf(stderr, "[simmy] screenshot capture was refused\n");
    else overlay_shot_requested();  // SIMMY_OVERLAY_TEST: paint the overlay into this shot
    if (g_shotPending) kbm_shot_requested();  // SIMMY_KBM_TEST: the mouse cross where the mouse is now
  }
  FrameStats& st = Stats();
  const auto handOver = std::chrono::steady_clock::now();
  if (st.on) st.bytes += aurora::gx::fifo::get_buffer_size();
  window2_present(split_windows_frame());  // the second player's window (pc_window2.cpp)
  aurora_end_frame();
  if (st.on) {
    const auto done = std::chrono::steady_clock::now();
    st.game += std::chrono::duration<double, std::milli>(handOver - st.begin).count();
    st.end += std::chrono::duration<double, std::milli>(done - handOver).count();
    if (++st.frames == 300) {
      std::fprintf(stderr, "[frame] %u: game %.2f ms, aurora end %.2f ms, gx %.0f bytes (averages)\n", g_frameNumber,
                   st.game / st.frames, st.end / st.frames, st.bytes / st.frames);
      st.game = st.end = st.bytes = 0;
      st.frames = 0;
    }
  }
  settings_frame_presented(devFrame);
  g_gameFramePresented = true;
  poll_screenshot();
}

// PC mod: network play (pc_net.cpp): while waiting for the other PC (connecting, or the partner is
// late), a frame with only the overlay (its notice line, the F1 options) so the window stays alive.
void platform_present_wait_frame() {
  if (g_headless || !aurora_begin_frame()) return;
  GXSetCopyClear(GXColor{0, 40, 48, 255}, GX_MAX_Z24);
  overlay_frame(0);
  aurora_end_frame();
}

void platform_frame_tick() {
  static unsigned int s_frames = 0;
  if (g_headless && ++s_frames % 300 == 0)
    std::fprintf(stderr, "[simmy] %u frames (gx fifo %u bytes)\n", s_frames, aurora::gx::fifo::get_buffer_size());
  platform_pump_events();
  if (!g_headless && !g_gameFramePresented && aurora_begin_frame()) {
    // Placeholder until the game renders its own frames: dark teal, so a live window is obvious.
    GXSetCopyClear(GXColor{0, 40, 48, 255}, GX_MAX_Z24);
    aurora_end_frame();
  }
  g_gameFramePresented = false;
  // Headless there is no aurora frame, so nothing ever takes the GX commands the game writes out of
  // aurora's command buffer (aurora_end_frame drains it); it grew every game frame (~40 KB a frame in
  // live mode) until aurora's 4 GB limit ("fifo::write_data: buffer size overflow" after ~60,000
  // frames). Headless they are dropped each frame. (With a window they must stay: commands written
  // between frames, e.g. the render-to-texture skin compositing, are processed by the next frame.)
  if (g_headless && aurora::gx::fifo::get_buffer_size() != 0) aurora::gx::fifo::clear_buffer();
  split_screen_end_frame();
  static unsigned s_tickCount = 0;
  ++s_tickCount;
  if (FramesFromHouse() && !g_houseTick && _globals.m_house) {
    g_houseTick = s_tickCount;
    g_housePresented = g_frameNumber + 1;
    std::fprintf(stderr, "[simmy] frame %u: house loaded, dev frames counted from here\n", g_housePresented);
  }
  const unsigned s_ticks = FromHouse(s_tickCount, g_houseTick);  // PC: SIMMY_FRAMES_FROM_HOUSE (0 while waiting)
  const DevOptions& dev = dev_options();
  if (dev.quitAfterFrames && s_ticks >= dev.quitAfterFrames && !g_shotPending && !g_quitRequested) {
    g_quitRequested = true;
    std::fprintf(stderr, "[simmy] --frames %u reached: quitting\n", dev.quitAfterFrames);
    if (g_onQuit) g_onQuit();
  }
  if (g_shotPending && s_ticks > dev.quitAfterFrames + 120) g_shotPending = false;  // give up eventually
  // ~60 Hz pacing (replaced by the renderer's retrace pacing once ported). PC fps_cap: a whole number
  // of 60 Hz retraces per frame, so the game's retrace-counted timing (EEngine::RetraceUpdate) keeps
  // its speed; it never runs faster than 60 Hz, whatever the display's refresh rate.
  if (det_frame_end()) return;  // SIMMY_DET_*: hashes, jitter; SIMMY_DET_NOPACE: no pacing (pc_det.cpp)
  // (PC mod: network play: always 60 frames a second, the game's speed on both PCs; fps_cap does not apply.)
  const auto next = g_lastTick + std::chrono::microseconds(net_session() ? 50000 / 3 : settings_frame_interval_us());
  std::this_thread::sleep_until(next);
  g_lastTick = std::max(next, std::chrono::steady_clock::now() - std::chrono::milliseconds(100));
}
}  // namespace simmy

namespace simmy {
#ifdef _WIN32
[[noreturn]] void process_exit_now(int code);  // pc_crash_win.cpp: TerminateProcess
void crash_reinstall_handlers();                // pc_crash_win.cpp
#endif

// The one way out once the game has run (every exit ends here: the F1 overlay's Quit, closing window 1,
// Alt+F4, SIMMY_DET_QUIT, --frames; closing player 2's window only hides it). By the time the game's
// main returns, its own exit path (EApp::Main -> EEngine::ShutdownThreads) has stopped the controllers,
// closed the audio device (its mixer thread is gone), finished the background thread's work (house loads,
// memory card saves) and joined it. Here: the second window, the overlay, then aurora (GPU device, SDL
// windows and subsystems), in that order; then the logs are flushed and the process ends at once.
//
// The process does not return from main: the game's global objects were made for a console that never
// exits (several destructors are unported, others free into heaps or join threads), so their static
// destructors are not run. On Windows the end is TerminateProcess on our own process, after the GPU
// device and windows are released: unlike ExitProcess it does not run the DLLs' detach code (d3d12 and
// driver DLLs with their threads killed mid-wait), which is where exit-time hangs and crashes come from;
// the files the game wrote are already closed (memory card commits are synchronous, simmy.ini is saved
// in place) and stdout / stderr are unbuffered. Elsewhere _Exit. The exit code is the game's.
[[noreturn]] void platform_shutdown_and_exit(int rc) {
  if (!g_headless) {
    window2_shutdown();
    overlay_shutdown();
    aurora_shutdown();
  }
  std::fprintf(stderr, "[simmy] clean exit (code %d)\n", rc);
  std::fflush(nullptr);
#if defined(__SANITIZE_ADDRESS__) && !defined(_MSC_VER)
  // Test builds with -fsanitize=address: _Exit skips LeakSanitizer's own exit check, so SIMMY_LSAN=1 runs
  // it here (memory still reachable from the game's globals is not reported).
  if (const char* lsan = std::getenv("SIMMY_LSAN"); lsan && *lsan == '1') {
    const int leaked = __lsan_do_recoverable_leak_check();
    std::fprintf(stderr, "[simmy] leak check: %s\n", leaked ? "leaks reported" : "no leaks");
    std::fflush(nullptr);
  }
#endif
#ifdef _WIN32
  process_exit_now(rc);
#else
  std::_Exit(rc);
#endif
}
}  // namespace simmy

static void log_callback(AuroraLogLevel level, const char* module, const char* message, unsigned int len) {
  static const char* names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
  std::fprintf(level >= LOG_ERROR ? stderr : stdout, "[%s %s] %.*s\n", names[level], module, (int)len, message);
  if (level == LOG_FATAL) {
    std::fflush(nullptr);
    std::abort();
  }
}

// The game's files: --data <dir> (development), else the disc image of --iso / iso= in simmy.ini,
// else the SIMMY_DATA folder (tests). Without any, or when the image is unusable, the player is told
// what to do (a message box on Windows, stderr everywhere) and the game does not start.
static bool open_game_data(const std::string& dataDir) {
  const std::string iso = simmy::graphics_settings().isoPath;
  std::string error;
  bool ok = false;
  if (!dataDir.empty()) {
    if (!iso.empty())
      std::fprintf(stderr, "[simmy] --data given: using the folder, not the disc image %s\n", iso.c_str());
    ok = simmy::data_source_open_folder(dataDir, error);
  } else if (!iso.empty()) {
    ok = simmy::data_source_open_disc(iso, error);
    if (!ok) error += "\n\nStart SimmyLauncher.exe to choose your disc image of The Sims 2.";
  } else if (const char* env = std::getenv("SIMMY_DATA"); env && *env) {
    ok = simmy::data_source_open_folder(env, error);
  } else {
    error = "SIMMY needs your disc image of The Sims 2 for the Nintendo GameCube (a .iso file).\n\n"
            "Start SimmyLauncher.exe and choose the disc image there; it starts the game for you.\n"
            "(Or run: simmy.exe --iso \"<path to the .iso>\")";
  }
  if (ok) return true;
  std::fprintf(stderr, "[simmy] cannot start: %s\n", error.c_str());
#ifdef _WIN32
  if (!simmy::g_headless && !simmy::dev_options().offscreen)
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SIMMY - The Sims 2", error.c_str(), nullptr);
#endif
  return false;
}

int main(int argc, char* argv[]) {  // aurora/main.h renames this to aurora_main
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--version") == 0) {
      std::printf("%s\n", SIMMY_PORT_VERSION);  // include/simmy/version.hpp
      return 0;
    } else if (std::strcmp(argv[i], "--mathcheck") == 0) {
      // The deterministic math's self-test (simmy/detmath.hpp): one hash per function, the same on
      // every build and CPU. Optional output file (simmy.exe may have no console).
      return simmy::dm::mathcheck(i + 1 < argc ? argv[i + 1] : nullptr);
    }
  std::fprintf(stderr, "[simmy] main\n");
  std::fprintf(stderr, "[simmy] %s\n", SIMMY_PORT_VERSION);
  // Unbuffered logs, so nothing is lost if the process is killed or crashes.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  std::vector<char*> args{argv[0]};
  bool fsTest = false;
  std::string dataDir;  // --data
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--headless") == 0) {
      simmy::g_headless = true;
    } else if (std::strcmp(argv[i], "--fs-test") == 0) {
      fsTest = true;
      simmy::g_headless = true;
    } else if (std::strcmp(argv[i], "--offscreen") == 0) {
      simmy::dev_options().offscreen = true;
    } else if (std::strcmp(argv[i], "--render-test") == 0) {
      simmy::dev_options().renderTest = true;
    } else if (std::strcmp(argv[i], "--qd-test") == 0) {
      simmy::dev_options().quickdataTest = true;
    } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 2 < argc) {
      simmy::dev_options().screenshotFrame = static_cast<unsigned>(std::atoi(argv[++i]));
      simmy::dev_options().screenshotPath = argv[++i];
    } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      simmy::dev_options().quitAfterFrames = static_cast<unsigned>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
      dataDir = argv[++i];
    } else if (simmy::net_parse_arg(argc, argv, i)) {
      // network play: --host, --join, --net-port, --net-delay (pc_net.cpp)
    } else if (simmy::settings_parse_arg(argc, argv, i)) {
      // a graphics setting (pc_settings.cpp)
    } else {
      args.push_back(argv[i]);
    }
  }

  simmy::settings_load(argc > 0 ? argv[0] : nullptr);  // also --iso / iso= (pc_settings.cpp)
  simmy::kbm_init();  // player 1's keys ([keys] in simmy.ini, pc_kbm.cpp)
  if (!open_game_data(dataDir)) return 1;
  if (!simmy::g_headless) {
    AuroraConfig config{};
    config.appName = "SIMMY";
    config.logCallback = &log_callback;
    config.imGuiInitCallback = &simmy::overlay_imgui_init;  // F1 options overlay: font and theme
    // Dev aid: SIMMY_GX_PROCESSING=thread|inline|drain (aurora's GX command processing; default thread).
    if (const char* m = std::getenv("SIMMY_GX_PROCESSING")) {
      if (!std::strcmp(m, "inline")) config.gxProcessingMode = GX_PROCESSING_INLINE;
      else if (!std::strcmp(m, "drain")) config.gxProcessingMode = GX_PROCESSING_DRAIN;
    }
    // Keep the GPU pipeline cache per build: a cache written by another build of the port can hold
    // pipelines this build no longer creates the same way (seen as "unmapped vtx attr" at startup).
    static std::string cacheDir;
    if (char* pref = SDL_GetPrefPath(nullptr, "SIMMY")) {
      // The stamp is the executable's size and write time, so every rebuild gets a fresh cache;
      // caches of older builds are removed.
      std::string stamp = "0";
      std::error_code ec;
      std::filesystem::path exe = argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path();
      if (const char* base = SDL_GetBasePath(); base && !exe.empty()) exe = std::filesystem::path(base) / exe.filename();
      if (!exe.has_extension() && !std::filesystem::exists(exe, ec)) exe += ".exe";
      const auto size = std::filesystem::file_size(exe, ec);
      if (!ec) {
        const auto time = std::filesystem::last_write_time(exe, ec).time_since_epoch().count();
        stamp = std::to_string(size) + "_" + std::to_string(static_cast<long long>(time));
      }
      const std::string name = "cache_" + stamp;
      for (const auto& entry : std::filesystem::directory_iterator(pref, ec)) {
        const std::string other = entry.path().filename().string();
        if (entry.is_directory(ec) && other.rfind("cache_", 0) == 0 && other != name)
          std::filesystem::remove_all(entry.path(), ec);
      }
      cacheDir = std::string(pref) + name;
      SDL_free(pref);
      if (SDL_CreateDirectory(cacheDir.c_str())) config.cachePath = cacheDir.c_str();
    }
    // Window size (the split-screen modes other than the original's and widescreen start with a
    // wider one, see platform.hpp), MSAA, anisotropy, vsync, backend, fullscreen: pc_settings.cpp.
    // SIMMY_BACKEND (offscreen tests) is read there too and now applies to windows as well.
    simmy::settings_apply_config(config, simmy::dev_options().offscreen);
    if (simmy::dev_options().offscreen) {
      config.surface.type = AURORA_SURFACE_HEADLESS;
      config.allowCpuAdapter = true;  // software rendering is fine for tests
      if (std::getenv("SIMMY_GPU_DEBUG")) config.logLevel = LOG_DEBUG;
    }
    const AuroraInfo info = aurora_initialize(argc, argv, &config);
    // The window's title carries the port's version (appName stays "SIMMY": aurora also names the
    // user and cache folders after it).
    if (SDL_Window* win = aurora_get_window()) SDL_SetWindowTitle(win, SIMMY_PORT_VERSION);
    std::fprintf(stderr, "[gfx] backend in use: %s\n", simmy::settings_backend_name(info.backend));
    simmy::settings_after_init(simmy::dev_options().offscreen);
    // The window opens centred (settings_apply_config). If it is taller than the screen's work
    // area, centring would push the title bar above the top edge: keep the whole window frame
    // inside the work area so it can always be grabbed and moved.
    if (SDL_Window* win = aurora_get_window(); win && !simmy::dev_options().offscreen &&
                                               !(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN)) {
      SDL_Rect area{};
      int x = 0, y = 0, top = 0, left = 0;
      if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(win), &area) && SDL_GetWindowPosition(win, &x, &y)) {
        SDL_GetWindowBordersSize(win, &top, &left, nullptr, nullptr);
        // Two-window co-op on a single display, with no remembered place for window 2: centre the
        // pair (window 2 opens 16 px right of window 1, see pc_window2.cpp) rather than window 1.
        int displays = 0;
        if (SDL_DisplayID* list = SDL_GetDisplays(&displays)) SDL_free(list);
        if (simmy::split_mode() == simmy::SplitMode::Windows && displays == 1 &&
            !simmy::graphics_settings().window2PosSet) {
          int w = 0, h = 0;
          SDL_GetWindowSize(win, &w, &h);
          const int pair = 2 * w + 16;
          if (pair <= area.w) x = area.x + (area.w - pair) / 2, SDL_SetWindowPosition(win, x, y);
        }
        const int minX = area.x + left, minY = area.y + top;
        if (x < minX || y < minY) SDL_SetWindowPosition(win, x < minX ? minX : x, y < minY ? minY : y);
      }
    }
    simmy::overlay_init(simmy::dev_options().offscreen);
#ifdef _WIN32
    simmy::crash_reinstall_handlers();  // SDL, Dawn or the driver may have replaced them
#endif
    // aurora only hands a controller to port 1 by default (ports 2-4 start as "none"), so a second
    // pad never reached the game and the front end offered only one player. Like a GameCube with
    // pads in several ports, give every unconfigured port the next free controller. A port the
    // player pinned to a specific controller is left alone.
    for (uint32_t port = 1; port < 4; ++port) {
      if (aurora::pad::port_preference(port).state == aurora::pad::PortAssignment::None)
        aurora::pad::set_port_auto(port);
    }
    // PC keyboard and mouse (pc_kbm.cpp): the keyboard is player 1's pad there, so aurora's own
    // keyboard-to-pad bindings (from its input profiles) are cleared, or a key would press twice. With
    // p1_input=keyboard_mouse, port 1 takes no controller: the first one goes to port 2 (player 2 in co-op).
    for (uint32_t port = 0; port < 4; ++port) {
      const auto target = aurora::pad::ProfileTarget::keyboard(port);
      if (const aurora::pad::Profile* prof = aurora::pad::profile(target)) {
        bool any = false;
        for (const auto& kv : prof->bindings) any = any || !kv.second.empty();
        if (any) std::fprintf(stderr, "[kbm] aurora keyboard bindings on port %u cleared (SIMMY's [keys] apply)\n", port + 1);
      }
      aurora::pad::clear_bindings(target);
    }
    if (simmy::kbm_p1_input() == 2) {
      aurora::pad::clear_port(0);
      if (aurora::pad::port_preference(1).state == aurora::pad::PortAssignment::None) aurora::pad::set_port_auto(1);
      std::fprintf(stderr, "[kbm] p1_input=keyboard_mouse: controllers start at port 2\n");
    }
  }

  if (fsTest) {
    // Read a few game files through the game's own file layer (EFileSystem -> ENgcFile).
    ENgcEngine::InitMemoryManager();
    _eorFileSys.Init(static_cast<int>(0x80000000u));
    for (const char* name : {"data/index.ind", "DATA/INDEX1.IND", "data/levels.arc", "data/missing.arc"}) {
      EFile* f = efopen(const_cast<char*>(name), const_cast<char*>("rb"));
      if (!f) {
        std::fprintf(stderr, "[fs-test] %-18s not found\n", name);
        continue;
      }
      unsigned char head[8] = {};
      const int n = efread(head, 1, 8, f);
      std::fprintf(stderr, "[fs-test] %-18s %8d bytes  name=%s ext=%s  first %d bytes:", name, f->GetLength(),
                   f->GetName(), f->GetExt(), n);
      for (int i = 0; i < n; ++i) std::fprintf(stderr, " %02X", head[i]);
      std::fprintf(stderr, "\n");
      efclose(f);
    }
    return 0;
  }

  // PC mod: network play (pc_net.cpp): connect, check, take the host's settings and memory card, before
  // the game's first frame (the session runs in lockstep from boot).
  if (const int rc = simmy::net_start_session(); rc != 0) simmy::platform_shutdown_and_exit(rc < 0 ? 0 : rc);

  // Dev switch: SIMMY_SKIP_INTRO=1 sets the config block's SkipEAMovie (+0x43C) and SkipIntroMovie
  // (+0x3A8) flags, so boot goes from the Maxis screen straight to the start screen. (A network session
  // skips them too unless its host says skip_intro = no.)
  if (const char* skip = std::getenv("SIMMY_SKIP_INTRO"); (skip && *skip && *skip != '0') || simmy::net_skip_intro()) {
    const int one = 1;
    std::memcpy(_globals.m_config + (0x43C - 0x168), &one, 4);
    std::memcpy(_globals.m_config + (0x3A8 - 0x168), &one, 4);
  }

  std::fprintf(stderr, "[simmy] entering game main\n");
  const int rc = simmy_game_main(static_cast<int>(args.size()), args.data());
  std::fprintf(stderr, "[simmy] game main returned %d; %u distinct unported functions were called\n", rc,
               simmy::stub_unique_count());

  simmy::platform_shutdown_and_exit(rc);
}
