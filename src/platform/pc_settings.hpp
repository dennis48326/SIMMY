// PC: graphics settings (simmy.ini next to the executable, environment, command line).
//
// Precedence, lowest first: built-in defaults (the GameCube picture: 4:3, the window's own resolution,
// no MSAA), simmy.ini (or --config <file>), SIMMY_GFX="key=value;key=value" and SIMMY_GFX_<KEY>
// environment variables, then command-line switches (--<key> <value> or --<key>=<value>). Keys and
// values are listed in simmy.ini.example (repo root); README.md has the user-facing summary.
#pragma once
#include <aurora/aurora.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace simmy {

enum class FullscreenMode { Off, Borderless, Exclusive };
enum class HudMode { Centered, Edges };

struct GraphicsSettings {
  unsigned windowWidth = 0, windowHeight = 0;  // 0 = default (aurora's 1280x960, or the aspect's)
  bool windowSet = false;                      // window= was given explicitly
  FullscreenMode fullscreen = FullscreenMode::Off;
  // Internal render resolution: 0 = auto (the window's own pixel size, as before). Otherwise the
  // EFB height in pixels (720, 1080, ...); the width follows the aspect.
  unsigned renderHeight = 0;
  float renderScale = 0.0f;  // the same as a factor of the GameCube's 640x480 (0 = auto)
  bool wide = false;         // aspect: false = 4:3 (GC), true = widescreen (the window's aspect)
  float defaultAspect = 4.0f / 3.0f;  // the aspect the default window size is made with
  HudMode hud = HudMode::Edges;  // widescreen: d-pad menu and needs to the screen edges
  unsigned msaa = 1;          // 1, 2, 4, 8
  unsigned anisotropy = 16;   // 1..16 (aurora's default: 16)
  bool vsync = false;         // as before: off (the game paces itself to 60 Hz)
  unsigned fpsCap = 60;       // 60, 30, 20 or 15: whole numbers of 60 Hz retraces per frame
  int backend = 0;            // AuroraBackend (BACKEND_AUTO)
  bool areaResampler = false; // aurora_set_resampler(SAMPLER_AREA) when downscaling a larger EFB
  // Co-op display (split=, --split): SplitMode (platform.hpp) 0 gc, 1 4x3, 2 wide, 3 windows.
  int split = 0;
  // The second player's window (split=windows, pc_window2.cpp): size (0 = window 1's), position and
  // display (remembered when the game quits), borderless fullscreen.
  unsigned window2Width = 0, window2Height = 0;
  bool window2Set = false;
  int window2X = 0, window2Y = 0;
  bool window2PosSet = false;
  int window2Display = -1;  // index in SDL's display list; -1 = auto
  bool fullscreen2 = false;
  std::string sourceFile;     // the ini that was read (empty if none)
  // Not graphics: the game's disc image (iso=, --iso; written by SimmyLauncher). Kept as given (case,
  // spaces, ';' and '#' are part of the path), surrounding quotes removed. Empty: none.
  std::string isoPath;
};

GraphicsSettings& graphics_settings();

// pc_main.cpp's argument loop: consumes a graphics switch at argv[i] (and its value), returning true.
bool settings_parse_arg(int argc, char** argv, int& i);
// Reads simmy.ini (next to the executable unless --config was given), the environment and the
// recorded switches, in that order. Call once, before settings_apply_config.
void settings_load(const char* argv0);
// Fills the AuroraConfig fields the settings control (window size, MSAA, anisotropy, vsync, backend,
// fullscreen). `offscreen` also sizes the headless surface.
void settings_apply_config(AuroraConfig& config, bool offscreen);
// After aurora_initialize: render scale, viewport policy, resampler, exclusive fullscreen.
void settings_after_init(bool offscreen);
// After each presented frame (test switch SIMMY_GFX_LOWRES_UNTIL, see pc_settings.cpp).
void settings_frame_presented(unsigned frame);
// Alt+Enter: windowed <-> the configured fullscreen mode (borderless if none is configured).
void settings_toggle_fullscreen();
const char* settings_backend_name(int backend);
// Microseconds between game frames: 60 Hz retraces times 60 / fps_cap.
long long settings_frame_interval_us();

// ---- In-game options overlay (pc_overlay.cpp) ----
// Applies, while the game runs, every difference between `before` and the current settings that
// aurora can change live: window size, aspect, internal resolution, fullscreen, vsync, anisotropy and
// the resampler (the HUD mode and the frame cap are read every frame). MSAA and the backend need a
// restart. Returns true if anything was applied.
bool settings_apply_live(const GraphicsSettings& before);
// One key=value as simmy.ini spells it (no live effect by itself; see settings_apply_live).
bool settings_set_value(const char* key, const std::string& value);
// The window's current fullscreen state (Alt+Enter, the overlay).
bool settings_fullscreen_now();
// The window size the settings ask for (window=, else the aspect's / split screen's default).
void settings_window_size(const GraphicsSettings& g, unsigned& width, unsigned& height);
// The split= values: "gc", "4x3", "wide", "windows".
const char* settings_split_name(int split);
// Writes only the given keys (simmy.ini spelling) to settings_ini_path() (the second window's
// geometry when the game quits), leaving every other line alone.
bool settings_save_keys(const char* const* keys, size_t count, std::string& message);
// The ini the overlay saves to: the file that was read (--config, SIMMY_INI), else simmy.ini next to
// the executable.
std::string settings_ini_path();
// Writes the settings to settings_ini_path(), keeping its comments, layout and unknown lines (a key's
// commented-out example line is replaced in place). Keys at their default are only rewritten where
// the file already sets them. `message` gets the result for the user.
bool settings_save(std::string& message);
// The value a key is written with ("16:9", "1080p", ...); `key` as in simmy.ini.
std::string settings_value_string(const GraphicsSettings& g, const char* key);

}  // namespace simmy
