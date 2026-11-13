// The port's version: the one place to bump it (see docs/CONTRIBUTING.md, "Port version").
//
// Shown in the game window's title, SimmyLauncher's window title and its home page, logged at startup
// by both, and printed by `simmy --version` / `SimmyLauncher --version`. Plain ASCII on purpose (a
// hyphen, not an em dash): the same bytes go to Windows title bars, consoles and log files.
// Plain macros (no C++), so any translation unit or a resource script can include it.
#ifndef SIMMY_VERSION_HPP
#define SIMMY_VERSION_HPP

#define SIMMY_PORT_BUILD "build eb"   // the delivery's name
#define SIMMY_PORT_DATE "2026-11-11"  // its date (YYYY-MM-DD)

// "SIMMY port - build eb (2026-11-11)"
#define SIMMY_PORT_VERSION "SIMMY port - " SIMMY_PORT_BUILD " (" SIMMY_PORT_DATE ")"

#endif
