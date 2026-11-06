# ov1: F1 in-game options overlay (Dear ImGui through aurora)

Tree /home/claude/gfx/port (merge base /home/claude/base65/port), build /home/claude/gfx/build (no reconfigure
needed beyond the automatic glob re-run). Full diff: $S/ov1/session_full.diff (gen/ excluded).
Scripts: $S/ov1/run.sh NAME LAST SHOTS [args] (offscreen, SIMMY_SKIP_INTRO=1, shots -> png),
$S/ov1/suite/{suite,hl}.sh (B-cancel suite, copied from g9, uses the gfx build).

## How it works
- aurora creates the ImGui context only when it has a window (lib/aurora.cpp initialize_ui, needs an XFB
  present target), runs ImGui::NewFrame in aurora_begin_frame and freezes/draws the draw data in
  aurora_end_frame's present pass (over the scaled picture, on the swapchain, NOT in the EFB).
  AuroraConfig::imGuiInitCallback = overlay_imgui_init: embedded font + theme + NavEnableKeyboard/Gamepad.
  overlay_frame(devFrame) is called in platform_end_frame before aurora_end_frame; it adds windows only
  while open and only when GImGui->WithinFrameScope.
- Input: keyboard/mouse reach ImGui via aurora's input router (ImGui layer); gamepads via ImGui's SDL3
  backend, switched to ImGui_ImplSDL3_GamepadMode_AutoAll so every pad navigates. F1/Esc come from the
  AURORA_SDL_EVENT stream (pc_main platform_pump_events -> overlay_handle_event).
- Game input blocked at the PC pad layer: overlay_filter_pads (ENgcControllerManager::Update after the dev
  scripts) neutralises buttons/sticks/triggers but keeps `err`, so pads stay "connected" (no
  disconnect prompts). It also takes B/Start/L/R edges from the game's pads (the GameCube B regardless of
  controller mapping; aurora's Xbox mapping puts PAD B on the West button). After closing, each pad stays
  blocked until fully released (the B/Start that closed the menu never reaches the game).
  One back/close per press (10-frame cooldown) because the pad and ImGui may both report it.
- Pause: ESimsApp::Update skips UpdateCheats/UpdateApt/UpdateGame/UpdateShaders while open; UpdateAudio,
  deferred images, UpdateDraw and the engine's PreFrameUpdate (clock, controllers) still run, so frames
  keep coming at the same 60 Hz pacing. Verified: two 1080p shots 35 frames apart while open are
  pixel-identical outside the window (even wall-clock animations freeze). Movies (EApp::SystemUpdate)
  keep playing if F1 is pressed during one.
- Mouse cursor: SDL_ShowCursor on open, restored on close. ImGui keyboard focus is dropped on close.
- Quit: Quit page -> modal confirm (default focus Cancel) -> platform_request_quit() = exactly the
  window-close path (ENgcEngine quit handler: s_bShuttingDown, m_quit) -> game main returns 0 ->
  overlay_shutdown -> aurora_shutdown.
- Look: navy #0a0a6e text, light-blue panels, #4a57c0 borders/header band, #3fb055 green fill on the
  selected row/button. Roboto Medium (Apache 2.0), subset to Latin-1 with pyftsubset, embedded as a
  compressed u32 array (src/platform/pc_overlay_font.inc, 40 KB source; no file to ship). Atlas holds
  13..72 px; font = window height / 30 picked from the atlas (crisp at 640x480 and 4K, checked
  3840x2160), style metrics scaled (borders too).

## Live vs restart
Live: aspect (also resizes the window to the aspect's default when window=auto; offscreen:
aurora_set_surface_size), HUD placement (read per frame), internal resolution (VISetFrameBufferScale,
0 = auto), window size (VISetWindowSize; exclusive fullscreen re-applies the display mode), fullscreen
off/borderless/exclusive (SDL, through windowed when switching modes), vsync (aurora_enable_vsync),
fps cap (read per tick), anisotropy (aurora::webgpu::g_graphicsConfig.textureAnisotropy; aurora builds
the sampler descriptor at every bind, samplers cached per descriptor -> the struct is redeclared in
pc_settings.cpp with aurora's exact definition), resampler (aurora_set_resampler).
After restart: MSAA (pipelines/attachments are built with the sample count), backend. Split screen
read-only. The fullscreen row shows the effective state (Alt+Enter included) and Save writes that.

## Saving
settings_save: rewrites the ini that was read (else exe dir simmy.ini; SDL_GetBasePath is UTF-8 and all
file IO now goes through SDL, so non-ASCII Windows paths work - LoadIni also switched from ifstream to
SDL_LoadFile). Active key lines get the new value (key spelling, spacing, trailing comment kept);
missing non-default keys replace their commented example line (";aspect = 4:3") in place, else are
appended; default values are written only where the file already sets the key; unknown lines kept;
CRLF on Windows; written to .tmp then SDL_RenamePath. Verified on simmy.ini.example + extra lines.

## Test path
Offscreen there is no aurora ImGui context, so with SIMMY_OVERLAY_TEST set the overlay creates its own,
uses DisplaySize = EFB size, feeds the pads (pre-block) as ImGui gamepad keys, keeps a copy of each
frame's draw lists and, when --screenshot/SIMMY_SHOTS captures, rasterises them on the CPU (font atlas,
bilinear, vertex colours, clip rects) into the BMP (both readback paths, incl. MSAA). The test context is
destroyed before aurora_shutdown (otherwise aurora's imgui::shutdown runs backend shutdown on it: crash,
seen and fixed). Actions: f1 open close up down left right a b start l r set:k=v save.

## Verification (llvmpipe, OpenGLES, offscreen)
- fe1/fe2: front end, overlay at 640x480; aspect 16:9 live (640x480 -> 1280x720 surface), resolution
  auto->480p->720p live, B closes.
- live1 (story start, FROM_HOUSE frames): 4:3 overlay, A on the aspect row -> 16:9 live, resolution
  480p/720p/1080p live (shot sizes 1280x720 -> 1920x1080 -> back), pause pixel-identical, Start/B close.
- final1: story start (welcome dialog), overlay open + A (dialog NOT dismissed: input blocked), Start
  closes (no pause menu), A with overlay closed dismisses the dialog (control), anisotropy live change
  logged, quit via Quit tab -> confirm -> Quit: "game main returned 0", exit 0, no card files written.
- quitfe: existing save (copied from s24) in the card folder, quit from the front end via the overlay:
  exit 0, save .gci md5 unchanged.
- save1: SIMMY_INI=test.ini, 16:9 + MSAA 4x via the pad, Save: only `aspect = 16:9` and `msaa = 4`
  changed (uncommented in place), `fps_cap = 30   ; I like 30` and `future_key = 42` kept; MSAA row shows
  "(restart to apply)" in red.
- msaa4 vs msaa1 (restart with the saved ini vs 16:9 msaa 1): ini read back (16:9, msaa 4, fps 30),
  edges anti-aliased (outputs/overlay_msaa_off_vs_on_after_restart.png); overlay composited in the MSAA
  screenshot path too.
- fe4k: 3840x2160 overlay crisp (72 px font).
- splitw: SIMMY_TWO_PLAYER=1 --split=wide, overlay over both views, Split screen row "wide", R switches
  page, B closes.
- Overlay closed: front end frame 500 bit-identical to the pre-session binary ($S/ov1/simmy_before).
- B-cancel suite (SIMMY_FUNDS=1000 SIMMY_STORY=0): toilet bladder 56.9, sofa comfort 28.3, shower
  hygiene 45.6, bed energy 31.8, cancel "cancels: 1" - same as g9.

## Not testable here
- Anisotropy: the live change is applied (logged) but llvmpipe shows no texture difference even between
  startup anisotropy 1 and 16 (diff only on wall-clock animated pixels), so its visual effect needs a GPU.
- Windowed mode: the real aurora ImGui path (swapchain draw, SDL3 backend input, gamepad AutoAll, mouse,
  cursor show/hide), F1/Esc key events, Alt+Enter, fullscreen borderless/exclusive switching,
  VISetWindowSize, vsync, the resampler (offscreen has no EFB->window scaling), backend choice, HiDPI
  framebuffer scale. The ImGui code is the same as in the test path; only its input/output plumbing differs.

## Known limitations
- While paused in 16:9, full-stage Apt art that is extended to the edges ("extend" HUD anchor) shows
  side bars until the game resumes if the aspect was switched while the menu is open (Apt display lists
  are rebuilt in UpdateApt, which is paused). 3D views update at once.
- Movies keep playing under the menu.
- ImGui's gamepad "windowing" (hold X/West) still works over the menu (harmless, one window).

## Files
New: src/platform/pc_overlay.cpp, pc_overlay.hpp, pc_overlay_font.inc.
Changed in src/platform: pc_main.cpp (imGuiInitCallback, overlay_init/shutdown, F1 events,
platform_request_quit, overlay_frame, screenshot composite hooks), pc_settings.cpp/.hpp (SDL file IO,
settings_apply_live/save/set_value/value_string/ini_path/window_size/fullscreen_now, anisotropy redecl).
Outside src/platform (vs base65):
- src/engine/e_ngcctrlmanager.cpp: 2 hunks (declaration; `simmy::overlay_filter_pads(m_pads);` after
  dev_pad_soak).
- src/game/appmain.cpp: 2 hunks (declaration of simmy::overlay_pauses_game; ESimsApp::Update skips
  UpdateCheats/UpdateApt/UpdateGame/UpdateShaders while paused).
- README.md (Graphics settings: "In-game options (F1)" paragraph), simmy.ini.example (2 comment lines).

## MSVC notes
No VLAs/designated initialisers/GNU extensions; explicit u16 casts; initializer_list std::min/max with
<algorithm>; font as unsigned int array (not a >64 KB string literal); ImTextureID is ImU64 in 1.91.9;
includes <backends/imgui_impl_sdl3.h> (imgui_headers exposes imgui_SOURCE_DIR; imgui_backends is linked
through aurora_core -> imgui PUBLIC). Windows aurora fetch is the same commit 505b5eb, ImGui 1.91.9b-docking.
