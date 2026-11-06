// PC: keyboard and mouse for player 1 (pc_kbm.cpp). The game-facing part (kbm_pads, kbm_tick,
// kbm_note_view, kbm_game_state) is in simmy/platform.hpp.
#pragma once
#include <SDL3/SDL_events.h>

#include <string>

namespace simmy {

// After the settings are read: the [keys] bindings (warnings for unknown names), the mode in the log.
void kbm_init();
// Every SDL event of window 1 (pc_main.cpp; player 2's window keeps its own): keys, mouse, focus.
void kbm_handle_event(const SDL_Event& event);
// The F1 overlay's Controls page: the actions, their labels and their keys as text.
int kbm_action_count();
const char* kbm_action_label(int action);
std::string kbm_binding_text(int action);
// Offscreen test shots (SIMMY_KBM_TEST): the mouse drawn as a small cross.
void kbm_shot_requested();  // the frame being captured: remember where the mouse is
void kbm_composite_shot(unsigned char* rgba, unsigned width, unsigned height);

}  // namespace simmy
