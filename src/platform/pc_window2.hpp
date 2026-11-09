// PC: the second player's window (co-op split=windows, pc_window2.cpp). The game-facing part
// (window2_present, window2_open, window2_pixel_size) is in simmy/platform.hpp.
#pragma once
#include <SDL3/SDL_events.h>

#include <string>

namespace simmy {

// Once a game frame after the events (platform_pump_events): opens the window when split=windows is
// chosen, closes it when another mode is, applies the overlay's requests.
void window2_frame();
// Every SDL event: true if it belongs to the second window (resize, move, close, Alt+Enter there);
// those are not for the game's window, the F1 options or the keyboard.
bool window2_handle_event(const SDL_Event& event);
// F2 in window 1, the options overlay: show the window again after it was closed, or hide it.
void window2_set_shown(bool shown);
bool window2_shown();
// Borderless fullscreen on the display the window is on (Alt+Enter in window 2, the options).
void window2_set_fullscreen(bool on);
bool window2_fullscreen();
// Before aurora_shutdown: remembers the window's place in simmy.ini, releases it.
void window2_shutdown();
// Screenshots (--screenshot, SIMMY_SHOTS) in split=windows: also <name>_p1 / <name>_p2 with what each
// window shows: its half of the frame (`windows`) scaled to the window's size, else the whole frame.
// PC mod: 4 players: `windows` is the number of strips of a split frame (0: not one), and every
// window gets its image (_p3, _p4).
void window2_write_shots(const std::string& path, const unsigned char* rgba, unsigned width, unsigned height,
                         int windows, void (*writeBmp)(const std::string&, const unsigned char*, unsigned, unsigned));

}  // namespace simmy
