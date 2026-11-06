// PC: the in-game options overlay (F1). A Dear ImGui window drawn through aurora's ImGui integration
// (aurora creates the context and runs NewFrame/Render around each presented frame and draws it over
// the presented picture). Pages: Graphics (every simmy.ini setting, applied live where aurora can),
// Controls (keys and buttons), Quit (through the game's normal exit path).
//
// While it is open the game gets no pad input (overlay_filter_pads, ENgcControllerManager::Update)
// and its update is skipped (overlay_pauses_game, ESimsApp::Update); it still draws every frame, so the
// 60 Hz pacing is unchanged.
//
// Test switch SIMMY_OVERLAY_TEST="<frame>:<action>,...": offscreen runs have no ImGui context (aurora
// only makes one for a window), so the overlay then runs in its own context, takes the pad as
// gamepad navigation, and is painted into --screenshot / SIMMY_SHOTS images by a small CPU
// rasteriser. Actions: f1 (toggle), open, close, up, down, left, right, a, b, start, l, r (a pad 1
// press of 3 frames), set:<key>=<value> (as if picked in the menu). Frames count like --screenshot
// (SIMMY_FRAMES_FROM_HOUSE applies).
#pragma once

#include <aurora/aurora.h>
#include <dolphin/pad.h>

union SDL_Event;

namespace simmy {

void overlay_imgui_init(const AuroraWindowSize* size);  // AuroraConfig::imGuiInitCallback
void overlay_init(bool offscreen);                      // after aurora_initialize
void overlay_shutdown();                                // before aurora_shutdown
void overlay_handle_event(const SDL_Event& event);      // every SDL event (F1, Escape)
void overlay_poll_keys();  // F1 from the keyboard state, once a frame after the events
void overlay_frame(unsigned devFrame);                  // each presented frame, before aurora_end_frame
bool overlay_is_open();
bool overlay_pauses_game();                             // ESimsApp::Update
void overlay_filter_pads(PADStatus* pads);              // ENgcControllerManager::Update, after PADRead
// Test path: a screenshot of this frame was requested / has arrived (RGBA, w x h).
void overlay_shot_requested();
void overlay_composite_shot(unsigned char* rgba, unsigned width, unsigned height);

// pc_main.cpp: leave the game the way closing the window does (the game's own shutdown).
void platform_request_quit();

}  // namespace simmy
