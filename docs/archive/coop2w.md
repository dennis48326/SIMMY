# coop2w: two-player co-op, one window per player (`--split=windows`)

Tree /home/claude/gfx/port (base71 copy). Build /home/claude/gfx_build (Ninja, clang, RelWithDebInfo). Its options
are the same as simmy_build's; for the real-window test it was briefly reconfigured with SDL_X11=ON and then put back.
SIMMY_AURORA_DIR=/home/claude/gfx/aurora is a copy of tools/aurora (the local readback/env test hacks are committed
there as one local commit) with the port's aurora patch applied.
Full diff vs base71 (gen/ and __pycache__ left out): $S/coop2w/session_full.diff. Aurora patch: $S/coop2w/0001-two-window-present.patch
(the same file as port/cmake/aurora_patches/). Deployable .bat: $S/coop2w/run_simmy_coop.bat (CRLF).
Outputs: /mnt/user-data/outputs/coop2w_p1.png, coop2w_p2.png, coop2w_both.png (1280x720 per window, MSAA 4, hud=edges,
lot 13, player 2's action menu open), coop2w_tests.png (pause menu in both windows, player 1 in social mode, real X11
windows of different sizes).

## Design
- **One EFB, two presents.** The game is unchanged: it still draws both halves into one 640x480 logical frame
  (ELiveMode::DrawMain / InitPlayerXWindow). With split=windows, during split frames:
  - The EFB is fixed (AuroraSetRenderSizeOverride, from the next frame on) at **2 x max(window widths) by
    max(window heights)** pixels, so each half has its window's native resolution (internal resolution= scales it).
    The STRETCH viewport policy maps logical 320 to the EFB's middle exactly.
  - Aurora's primary present (window 1) shows the **left half**. Window 2's present target gets the **right half**
    through `present_to`. Both use `set_present_crop`, a stretched source rectangle. Different window aspects work,
    because each half is projected for its own window (see the next item).
  - **Aspect per player.** InitPlayerXWindow calls `split_screen_player(i)`, and `GetScreenAspect()` then returns
    2 x the aspect of that player's window. The game's own `* 0.5` gives each half the window's aspect: 3D is Hor+,
    with the same vertical FOV, exactly like 1P widescreen. Mirror portals and the censor block size follow too.
  - **2D undistorted per window** (display_map_viewport -> MapWindows). Each 2D draw goes to a window, with content
    laid out over half the stage (320x480, the GC two-player HUD) or the whole stage (640x480 dialogs). The content
    is fitted at the window's height into that window's half of the EFB, with anchors centre, stretch, bleed (with a
    projection fix-up), left edge and right edge. The scissor is clamped to the window's half, so nothing leaks into
    the other window. A perspective draw whose aspect matches its window is left alone (the player views).
  - **Apt HUD routing** (pc_hud.cpp, WindowOf). Each instance is assigned top down:
    - in the stage's left or right half: player 1's or player 2's window;
    - something drawn across the middle (a dialog, the pause menu, a dimmer): **both windows**. AptDisplayList::render
      draws that instance twice (simmy_apt_hud_enter returns 2; simmy_apt_hud_next_pass switches the window);
    - a container as wide as the stage, or one with parts on both sides and nothing across the middle: its children
      decide. Example: 0MOD's clock-and-money bar, where the clock goes to P1 and the money to P2, as the GC split showed.
    Text fields are measured by their text, not their box (P1's clock field box reaches to x=733). hud=centered or
    hud=edges then places things inside the window as in 1P widescreen. hud=edges keeps a split group's parts
    together (the money icon stays with its box).
  - No divider bar between the halves (split_draws_divider). Player 1's half is selected afterwards, as the divider
    left it.
- **Outside split frames** (front end, CAS, loading, 1P, build/buy, a full-screen single view): EFB sizing is
  automatic again. Window 1 is exactly as before, and **window 2 mirrors window 1** (present_to without a crop,
  aspect fitted). This is useful because the two-player menus, player 2's Create-a-Sim and build/buy are full-screen
  on the GC too. It also costs only one resample.
- **Window 2** (pc_window2.cpp): an SDL window plus a WebGPU surface on aurora's device
  (`create_window_surface(device.GetAdapter().GetInstance(), ...)`), registered as an aurora present target.
  - It is **never vsynced** (mailbox, else immediate), so window 1's vsync paces the game. There is still one
    blocking swap a frame.
  - Placement: the remembered window2_pos if it is still on a display; else window2_display; else a display other
    than window 1's; else to the right of window 1.
  - Resizing, moving, DPI changes and display changes resize the target. Alt+Enter in window 2 toggles borderless
    fullscreen on its display.
  - Closing window 2 **hides** it. Window 1 then falls back to the wide split, both halves in window 1. F2 in window 1
    or F1 > Graphics > "Player 2 window" shows it again.
  - Closing window 1 quits as before. Window 2 is destroyed before aurora_shutdown.
  - Size, position, display and fullscreen are written to simmy.ini (only those four keys, settings_save_keys) when
    the window is hidden, when the mode is left, or when the game quits, and only if they changed.
  - Background joystick input is turned on while window 2 exists, so both pads work whichever window (or program)
    has focus. Keyboard, mouse, ImGui and F1 only take window 1's events. overlay_poll_keys only runs while window 1
    has the keyboard focus in this mode. Window 2's events are not routed to the game.
- **Settings.** split is now an ordinary graphics setting: split= in simmy.ini, --split=X or --split X,
  SIMMY_SPLIT, SIMMY_GFX_SPLIT. The old pc_main parsing was removed and the CLI still wins.
  - New keys: window2, window2_pos, window2_display, fullscreen2.
  - F1 Graphics: the "Co-op display" row is now a live choice (it was read-only). A "Player 2 window" row
    (Windowed, Fullscreen, Hidden) shows in windows mode.
  - "Reset to defaults" keeps the co-op layout and window 2's place.
  - README and simmy.ini.example are updated.
- **Offscreen and dev.** In offscreen runs there is a virtual window 2 (window2= size, default window 1's size).
  Every --screenshot or SIMMY_SHOTS image in this mode also writes `<name>_p1` and `<name>_p2`: what each window
  shows (its half bilinearly scaled to the window size, or the whole frame when mirroring).
  - SIMMY_WIN2_TRACE=<frame> or d<devframe> logs every 2D mapping.
  - SIMMY_HUD_TRACE also takes d<devframe> now and logs the window decisions.
  - Two new functions: platform_dev_frame and dev_trace_frame.

## Aurora patch (cmake/aurora_patches/0001-two-window-present.patch, about 100 lines, 10 files)
- `AuroraSetRenderSizeOverride(w, h)` (GXAurora.h; efb.cpp apply_override in prepare_frame, pending_size and
  resolve_size).
- `aurora::gfx::set_present_crop(target, u0, v0, u1, v1)` (gfx.hpp; TargetState.crop; present::current_crop).
  record_present reads it live, so it applies to the frame being recorded: the viewport covers the whole surface and
  the resample is forced. The resample shader gets a `uv_rect` uniform (offset/size; with (0,0,1,1) the output is
  identical to before, for both the bilinear and the area paths).
- With no crop or override set, aurora behaves exactly as before. That is checked by the 1P and gc/4x3/wide
  comparisons below.
- **How it is applied:** cmake/SimmyAuroraPatches.cmake, `simmy_patch_aurora(dir)`, runs at configure time. For
  each patch, `git apply --reverse --check` means it is already applied and is skipped; otherwise it runs
  `git apply --ignore-whitespace`, and stops with a clear message if the tree is not SIMMY_AURORA_TAG.
  - FetchContent now uses `SOURCE_SUBDIR simmy-populate-only` (download only), patches, and then calls
    add_subdirectory itself.
  - SIMMY_AURORA_DIR trees are patched too (option SIMMY_AURORA_PATCH, ON by default).
  - Tested: a fresh FetchContent clone from GitHub; reconfigure (skipped); an existing unpatched _deps/aurora-src,
    as in the user's build folder (patched); core.autocrlf=true CRLF checkout, as on Windows (patched, then
    skipped); the patch against a clean `git archive 505b5eb`.

## Verification
| run | what | result |
|---|---|---|
| w1/w5 (offscreen, windows, 640x360 each) | 2P lot 13 | EFB 1280x360. P1: clock and wheel; P2: money and wheel; each view 16:9; welcome dialog in both windows |
| w6 (hud=edges) | both cameras moved independently, P1 action menu | wheels at the window edges, money icon with its box, menu list in P1's window |
| w7 | P2 action menu (Sink/Fridge) | in window 2 only |
| w8 | pause menu (P1 start) | "Player 1" pause bar and dimmer in **both** windows (4:3 size, centred) |
| w11 | P1 Socialize P2's sim -> social mode | social menu, interest meter, "Goodbye B" and hourglass in window 1; the social camera zooms in P1's window only |
| story (SIMMY_CAS_ACCEPT=300) | 2P story start, lot 9 | tutorial dialog in both windows, both sims |
| bb | P1 pause -> buy mode ("Grab Mode") | single view: EFB automatic again (480x360 FIT); window 2 mirrors |
| hq (1280x720 each, MSAA 4, aniso 16, hud=edges) | MSAA screenshot path | EFB 2560x720; p1/p2 at 1280x720 (outputs) |
| **X11, Xvfb + real SDL windows** (OpenGL ES backend, x11run.sh / xh.c) | two real windows | window 2 opened at 656,0 beside window 1. Front end mirrored. 2P live: window 1 (640x480, 4:3) and window 2 (640x360, 16:9) each undistorted (different aspects). WM_DELETE on window 2 -> hidden, window 1 shows the wide split. F2 -> back. Resize to 800x450 -> EFB 1600x480, picture correct. Closing window 1 -> "game main returned 0"; ini has window2=800x450, window2_pos=700,380, window2_display=0. (Alt+Enter could not be tested: Xvfb has no window manager and the synthetic Alt state is not seen) |
| r1p new vs base71 (1P 16:9 960x540) | 1P unaffected | same framing and HUD (animation only) |
| rs_{gc,4x3,wide} new vs base71 (2P) | old split modes unaffected | same framing, divider and HUD |
| suite ($S/coop2w/suite) | toilet 57.2, sofa 28.3, fridge 31.7, shower 46.3, bed 32.7; `grep -c "cancels: 1" s_cancel.log` = 1 | 0 script errors, TREEASSERT or "keeps failing" |

## MSVC and Windows notes
No VLAs or GNU extensions, no operator== or `using` changes. SDL3 calls used:
SDL_CreateWindowWithProperties (with EXTERNAL_GRAPHICS_CONTEXT as aurora's window), SDL_GetDisplays/UsableBounds,
SDL_SetWindowFullscreenMode(nullptr), SDL_GetWindowFromEvent. The Dawn surface comes from aurora's own
create_window_surface (HWND on Windows). On D3D12 the second swap chain is created and presented on aurora's render
thread, like the first.

## Known limitations
- If the windows differ in size, the EFB uses the larger size for both halves, and the smaller window is downsampled
  (area or bilinear resampler). Equal windows (the default) are 1:1.
- The first frame after entering or leaving the split view is drawn at the previous EFB size (one frame of lower
  resolution).
- Dialogs over the whole stage (pause menu, story or tutorial messages) are shown in both windows, as both players
  saw them on the GC TV. A player-specific full-stage dialog (0G2D/1G2D) also appears in the other player's window.
- hud=edges inside a window uses the 1P heuristics, scaled to the half. The action menu's list stays centred while
  the wheel goes to the edge, as in 1P edges.
- Window 2 is not vsynced (mailbox/immediate). On a driver without mailbox it can tear.
- Alt+Enter fullscreen and multi-monitor placement were not exercised here (Xvfb has one screen and no window
  manager).
- Window 2 has no F1 overlay and takes no keyboard input, by design.
