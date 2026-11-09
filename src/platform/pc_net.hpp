// PC mod: network play (pc_net.cpp): the parts pc_main.cpp, pc_settings.cpp and pc_overlay.cpp use.
// The game-facing part (net_active, net_pads, net_frame_end, ...) is in simmy/platform.hpp.
#pragma once
#include <string>

namespace simmy {

// --host [port], --join <address>[:port], --net-port <port>, --net-delay <ticks> (consumes them).
bool net_parse_arg(int argc, char** argv, int& i);
// A line of simmy.ini's [net] section (host=, join=, port=, delay=, hash_every=, timeout=, on_end=,
// skip_intro=). `where` is "file:line" for messages.
void net_ini_value(const std::string& key, const std::string& value, const std::string& where);
// After aurora_initialize (or headless), before the game starts: connects, checks that both PCs can
// play together, takes the host's settings and memory card (client). 0: go on (also without a
// session); otherwise the process exit code (refused, failed; -1: cancelled by the player, exit 0).
int net_start_session();
// The session skips the intro movies (the host's choice, skip_intro=, default yes).
bool net_skip_intro();
// A short text for the overlay's notice line (waiting for the partner, the session's end); empty: none.
const std::string& net_notice();

}  // namespace simmy
