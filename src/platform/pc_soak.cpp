// PC only: the soak-test driver (no GameCube counterpart).
//
// SIMMY_SOAK=<seed> turns it on. It plays pad 1 (and pad 2 in two-player games) like an unattended
// player: once a house is loaded (and SIMMY_SOAK_START frames, default 300) it fires a random "macro"
// every SIMMY_SOAK_EVERY frames on average (default 150): walks, camera moves, A on what is near (and
// the first menu item), B, Y (next sim), X, Z (classic cursor control and back), the d-pad, L/R, the
// pause menu and its tabs, buy and build mode open and close, and (weight SIMMY_SOAK_QUIT, default 1,
// 0 never) quit to the main menu. Every macro is logged as "[soak] frame F: name". SIMMY_SOAK_P2=0
// keeps the macros on pad 1 in two-player games. SIMMY_SOAK_SPEED=-1/-2/-3 sets the game speed
// (x0.5, x5, x30) at the house load and every 3000 frames.
//
// SIMMY_SOAK_LOT=<1..16> (with SIMMY_DEV_MENU=1) also drives the main menu's development menu to that
// lot: start on the title screen, Y on the main menu (neighbourhood list), SIMMY_SOAK_NGH (0..5,
// default 0) presses of SIMMY_SOAK_DOWN (default "down"; the movie's lists move with "ldown") and A,
// then lot - 1 presses and A. After a quit it enters the lot again. Without it another pad script
// gets to the house.
//
// It also watches the run: "[soak] rss" every SIMMY_SOAK_RSS frames (default 1000) with the resident
// set size, "[soak] slow frame" for a frame over 2 s, and a watchdog thread that logs "[soak] HANG" and
// aborts (so a debugger shows every thread) when the frame counter has not moved for SIMMY_SOAK_HANG
// seconds (default 90). SIMMY_SOAK_STALL=<seconds> raises SIGUSR1 once a frame has taken that long
// (meant for runs under a debugger: it stops there and shows the threads).
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <atomic>
#include <csignal>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <random>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace simmy {

namespace {
struct Step {
  std::string names;  // "a", "lup", "p2:b", "a+b"; empty = nothing pressed
  int hold;           // frames pressed
  int gap;            // frames after release
};

struct SoakState {
  bool on = false;
  std::mt19937 rng;
  int every = 150;
  int rssEvery = 1000;
  int hangSec = 90;
  int lot = 0;
  int ngh = 0;
  bool p2 = true;
  // menu driver
  int phase = 0;
  int phaseSince = 0;
  int lastPress = -1000;
  int lastField = -1;
  int downs = 0;            // list presses made
  std::string down = "down";  // SIMMY_SOAK_DOWN: the button that moves the development menu's lists
  // macros
  std::vector<Step> steps;
  size_t step = 0;
  int stepStart = 0;
  int nextAt = 0;
  int houseSince = -1;
  int macros = 0;
  int entries = 0;
  int quitWeight = 1;    // SIMMY_SOAK_QUIT: the quit macro's weight (0 never)
  int speed = 0;         // SIMMY_SOAK_SPEED
  int startDelay = 300;  // SIMMY_SOAK_START: frames from the house load to the first macro
};

SoakState& St() {
  static SoakState s;
  return s;
}

int EnvInt(const char* n, int def) {
  const char* v = std::getenv(n);
  return v && *v ? std::atoi(v) : def;
}

void Press(PADStatus* pads, const std::string& names) {
  static const struct {
    const char* name;
    u16 bit;
  } kButtons[] = {{"start", PAD_BUTTON_START}, {"a", PAD_BUTTON_A}, {"b", PAD_BUTTON_B}, {"x", PAD_BUTTON_X},
                  {"y", PAD_BUTTON_Y}, {"up", PAD_BUTTON_UP}, {"down", PAD_BUTTON_DOWN},
                  {"left", PAD_BUTTON_LEFT}, {"right", PAD_BUTTON_RIGHT},
                  {"l", PAD_TRIGGER_L}, {"r", PAD_TRIGGER_R}, {"z", PAD_TRIGGER_Z}};
  size_t pos = 0;
  while (pos <= names.size()) {
    const size_t plus = names.find('+', pos);
    std::string name = names.substr(pos, plus == std::string::npos ? std::string::npos : plus - pos);
    PADStatus* pad = &pads[0];
    if (name.compare(0, 3, "p2:") == 0) {
      pad = &pads[1];
      name.erase(0, 3);
    }
    pad->err = PAD_ERR_NONE;
    for (const auto& b : kButtons)
      if (name == b.name) pad->button |= b.bit;
    if (name == "lup") pad->stickY = 127;
    if (name == "ldown") pad->stickY = -127;
    if (name == "lleft") pad->stickX = -127;
    if (name == "lright") pad->stickX = 127;
    if (name == "cup") pad->substickY = 127;
    if (name == "cdown") pad->substickY = -127;
    if (name == "cleft") pad->substickX = -127;
    if (name == "cright") pad->substickX = 127;
    if (plus == std::string::npos) break;
    pos = plus + 1;
  }
}

long RssKb() {
#if defined(__linux__)
  if (FILE* f = std::fopen("/proc/self/statm", "r")) {
    long size = 0, rss = 0;
    const int n = std::fscanf(f, "%ld %ld", &size, &rss);
    std::fclose(f);
    if (n == 2) return rss * (sysconf(_SC_PAGESIZE) / 1024);
  }
#endif
  return -1;
}

std::atomic<int> g_watchFrame{0};

void Watchdog(int hangSec, int stallSec) {
  int last = -1;
  int still = 0;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const int f = g_watchFrame.load();
    if (f != last) {
      last = f;
      still = 0;
      continue;
    }
    if (stallSec > 0 && still + 1 == stallSec) {
      // Under a debugger: stop here so it shows where the main thread is stuck (the run ends).
      std::printf("[soak] STALL: frame %d has not moved for %d s\n", f, stallSec);
      std::fflush(stdout);
#if defined(SIGUSR1)
      std::raise(SIGUSR1);
#else
      std::raise(SIGABRT);  // (no SIGUSR1 on Windows: a debugger stops on the abort instead)
#endif
    }
    if (++still >= hangSec) {
      std::printf("[soak] HANG: frame %d has not moved for %d s\n", f, still);
      std::fflush(stdout);
      std::abort();
    }
  }
}

int Rand(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(St().rng); }

const char* Pick(std::initializer_list<const char*> l) {
  const int i = Rand(0, static_cast<int>(l.size()) - 1);
  return *(l.begin() + i);
}

// One random macro, its name logged.
void NewMacro(int frame) {
  SoakState& s = St();
  s.steps.clear();
  s.step = 0;
  const bool two = dev_two_player() && s.p2;
  const std::string pre = two && Rand(0, 99) < 40 ? "p2:" : "";
  auto add = [&](const std::string& n, int hold, int gap) { s.steps.push_back({n.empty() ? n : pre + n, hold, gap}); };
  static const struct {
    const char* name;
    int weight;
  } kMacros[] = {{"walk", 20}, {"camera", 6}, {"interact", 12}, {"interact2", 6}, {"b", 10}, {"y", 6},
                 {"x", 3},     {"classic", 6}, {"pause", 6},      {"pausetab", 6}, {"buy", 5}, {"dpad", 4},
                 {"lr", 3},    {"wait", 4},    {"build", 3},     {"quit", s.quitWeight}};
  int total = 0;
  for (const auto& m : kMacros) total += m.weight;
  int r = Rand(0, total - 1);
  const char* name = kMacros[0].name;
  for (const auto& m : kMacros) {
    if (r < m.weight) {
      name = m.name;
      break;
    }
    r -= m.weight;
  }
  const std::string n = name;
  const char* stick = Pick({"lup", "ldown", "lleft", "lright", "lup+lleft", "lup+lright", "ldown+lleft", "ldown+lright"});
  if (n == "walk") add(stick, Rand(20, 200), 10);
  else if (n == "camera") add(Pick({"cup", "cdown", "cleft", "cright"}), Rand(10, 90), 10);
  else if (n == "interact") add("a", 2, 90), add("a", 2, 60);
  else if (n == "interact2") add("a", 2, 90), add("down", 2, 30), add("a", 2, 60);
  else if (n == "b") add("b", 2, 30);
  else if (n == "y") add("y", 2, 60);
  else if (n == "x") add("x", 2, 60);
  else if (n == "classic") {
    add("z", 2, 60);
    for (int i = Rand(1, 4); i > 0; --i) add(stick, Rand(5, 40), 20), add("a", 2, 60);
    add("a", 2, 60);
    if (Rand(0, 2)) add("z", 2, 60);
  } else if (n == "pause") add("start", 2, Rand(60, 240)), add("b", 2, 60);
  else if (n == "pausetab") {
    add("start", 2, 120);
    for (int i = Rand(0, 5); i > 0; --i) add(Pick({"right", "left"}), 2, 40);
    add("a", 2, 200);
    for (int i = Rand(0, 3); i > 0; --i) add(Pick({"up", "down", "left", "right", "r", "l"}), 2, 40);
    add("b", 2, 90), add("b", 2, 90), add("b", 2, 60);
  } else if (n == "buy" || n == "build") {
    add("start", 2, 120);
    add(n == "buy" ? "@paz:main_catalog_buy" : "@paz:main_catalog_build", 0, 0);
    add("a", 2, 300);
    for (int i = Rand(0, 3); i > 0; --i) add(Pick({"down", "right", "r"}), 2, 40);
    if (Rand(0, 1)) {
      add("a", 2, 120);  // pick it up (place mode)
      add(stick, Rand(5, 30), 30);
      add(Pick({"a", "b"}), 2, 120);  // place it, or put it back
    }
    add("b", 2, 120), add("b", 2, 120), add("b", 2, 90);
  } else if (n == "quit") {
    // The options tab, its last item (quit), yes: back to the main menu (the menu driver re-enters).
    add("start", 2, 120);
    add("@paz:main_game_options", 0, 0);
    add("a", 2, 300), add("down", 2, 100), add("a", 2, 200), add("up", 2, 100), add("a", 2, 300);
    add("b", 2, 120), add("b", 2, 120);  // (when it was not quit after all)
  } else if (n == "dpad") add(Pick({"up", "down", "left", "right"}), 2, 60);
  else if (n == "lr") add(Pick({"l", "r"}), Rand(2, 30), 40);
  else add("", Rand(100, 600), 0);
  ++s.macros;
  std::printf("[soak] frame %d: %s%s\n", frame, pre.c_str(), name);
}

// The development menu to SIMMY_SOAK_LOT.
void DriveMenu(PADStatus* pads, int frame) {
  SoakState& s = St();
  MMUTarget* mmu = MMUTarget::m_MMUSingleton;
  auto press = [&](const char* n, int phase) {
    if (frame - s.lastPress < 2) {
      Press(pads, n);
      return;
    }
    if (frame - s.lastPress < 120) return;
    s.lastPress = frame;
    Press(pads, n);
    std::printf("[soak] frame %d: menu %s (phase %d)\n", frame, n, phase);
  };
  switch (s.phase) {
    case 0:  // title: start until the main menu is up
      if (mmu && mmu->m_ready) {
        s.phase = 1, s.phaseSince = frame;
        break;
      }
      if (frame > 300) press("start", 0);
      break;
    case 1:  // main menu: Y opens the neighbourhood list
      if (!mmu) break;
      if (mmu->m_field8C == 1) {
        s.phase = 2, s.phaseSince = frame;
        break;
      }
      if (frame - s.phaseSince > 150) press("y", 1);
      break;
    case 2:  // neighbourhood
      if (!mmu) break;
      if (mmu->m_field8C == 2) {
        s.phase = 3, s.phaseSince = frame;
        break;
      }
      if (frame - s.phaseSince < 90) break;
      // The movie keeps its own highlight (MMU_NGH_INDEX / MMU_LOT_INDEX come from it): count presses.
      if (frame - s.lastPress >= 120 && s.downs < s.ngh) ++s.downs, press(s.down.c_str(), 2);
      else press(s.downs < s.ngh ? s.down.c_str() : "a", 2);
      break;
    case 3:  // lot
      if (!mmu || mmu->m_field8C != 2) {
        if (!mmu || mmu->m_field8C == 0) {
          s.phase = 4, s.phaseSince = frame;
          std::printf("[soak] frame %d: lot launched (start lot %d)\n", frame, static_cast<int>(_app.GetStartLot()));
        }
        break;
      }
      if (frame - s.phaseSince < 90) break;
      if (s.lastField != 3) s.lastField = 3, s.downs = 0;
      if (frame - s.lastPress >= 120 && s.downs < s.lot - 1) ++s.downs, press(s.down.c_str(), 3);
      else press(s.downs < s.lot - 1 ? s.down.c_str() : "a", 3);
      break;
    default:
      break;
  }
}
}  // namespace

void dev_pad_soak(PADStatus* pads) {
  SoakState& s = St();
  static bool init = false;
  if (!init) {
    init = true;
    const char* seed = std::getenv("SIMMY_SOAK");
    if (!seed) return;
    s.on = true;
    s.rng.seed(static_cast<unsigned>(std::strtoul(seed, nullptr, 0)));
    s.every = EnvInt("SIMMY_SOAK_EVERY", 150);
    s.quitWeight = EnvInt("SIMMY_SOAK_QUIT", 1);
    s.speed = EnvInt("SIMMY_SOAK_SPEED", 0);
    s.startDelay = EnvInt("SIMMY_SOAK_START", 300);
    s.rssEvery = EnvInt("SIMMY_SOAK_RSS", 1000);
    s.hangSec = EnvInt("SIMMY_SOAK_HANG", 90);
    s.lot = EnvInt("SIMMY_SOAK_LOT", 0);
    s.ngh = EnvInt("SIMMY_SOAK_NGH", 0);
    s.p2 = EnvInt("SIMMY_SOAK_P2", 1) != 0;
    if (const char* d = std::getenv("SIMMY_SOAK_DOWN")) s.down = d;
    if (s.lot <= 0) s.phase = 4;
    std::printf("[soak] seed %s lot %d ngh %d every %d\n", seed, s.lot, s.ngh, s.every);
    std::thread(Watchdog, s.hangSec, EnvInt("SIMMY_SOAK_STALL", 0)).detach();
  }
  if (!s.on) return;
  const int frame = _framecount;
  g_watchFrame.store(frame);

  static auto lastWall = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  const double dt = std::chrono::duration<double>(now - lastWall).count();
  lastWall = now;
  if (dt > 2.0) std::printf("[soak] slow frame %d: %.1f s\n", frame, dt);
  if (s.rssEvery > 0 && frame % s.rssEvery == 0)
    std::printf("[soak] rss frame %d: %ld KB house %d macros %d\n", frame, RssKb(), _globals.m_house ? 1 : 0, s.macros);

  if (s.phase < 4) {
    DriveMenu(pads, frame);
    return;
  }
  if (!_globals.m_house) {
    if (s.houseSince >= 0) {
      std::printf("[soak] frame %d: house gone\n", frame);
      ++s.entries;
      // Back on the main menu (quit): the development menu again, into the same lot.
      if (s.lot > 0) s.phase = 0, s.phaseSince = frame, s.lastField = -1, s.downs = 0;
    }
    s.houseSince = -1;
    s.steps.clear();
    s.step = 0;
    return;
  }
  if (s.houseSince < 0) {
    s.houseSince = frame;
    s.nextAt = frame + s.startDelay;
    std::printf("[soak] frame %d: house up\n", frame);
  }
  // SIMMY_SOAK_SPEED=-1/-2/-3: the game speed (x0.5, x5, x30), set at house load and every 3000 frames.
  if (s.speed && Globs_pSimulator && (frame == s.houseSince || (frame - s.houseSince) % 3000 == 0) &&
      Globs_pSimulator->GetSpeed() != s.speed) {
    Globs_pSimulator->SetSpeed(SimSpeed{s.speed});
    std::printf("[soak] frame %d: speed %d\n", frame, s.speed);
  }
  if (s.step >= s.steps.size()) {
    if (frame < s.nextAt) return;
    NewMacro(frame);
    s.stepStart = frame;
  }
  if (s.step >= s.steps.size()) return;
  const Step& st = s.steps[s.step];
  const int t = frame - s.stepStart;
  if (st.names.compare(0, 5, "@paz:") == 0) {
    // Right on the pause menu until its tab `name` is under the cursor (at most 16 presses).
    auto* paz = static_cast<PAZTarget*>(_gd.m_field70);
    const bool there = paz && paz->m_items && paz->m_count && paz->m_items[paz->m_selected % paz->m_count].name &&
                       st.names.compare(5, std::string::npos, paz->m_items[paz->m_selected % paz->m_count].name) == 0;
    if (there || t > 16 * 40 || (!paz && t > 200)) {
      ++s.step;
      s.stepStart = frame;
    } else if (paz && t >= 60 && t % 40 < 2) {
      Press(pads, "right");
    }
    return;
  }
  if (t < st.hold) {
    if (!st.names.empty()) Press(pads, st.names);
  } else if (t >= st.hold + st.gap) {
    ++s.step;
    s.stepStart = frame;
    if (s.step >= s.steps.size()) s.nextAt = frame + Rand(s.every / 3, s.every * 5 / 3);
  }
}

}  // namespace simmy
