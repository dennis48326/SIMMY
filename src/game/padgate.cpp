// PC development aid (no GC counterpart): SIMMY_PAD_GATE holds pad 1's scripted presses in the house
// until the game is ready for them, so a headless story run does not depend on fixed waits.
//
// SIMMY_PAD_GATE="tok,tok,...": once a house is loaded, every press on pad 1 (a press = the frames from
// a button or stick leaving neutral until it is neutral again, as SIMMY_PAD_SEQ / SIMMY_PAD_SCRIPT made
// it) is taken off the pad and queued; the n-th one is played back (frame by frame, as recorded) when
// the n-th token's condition holds, at least as many frames after the previous one as it came after it.
// Tokens ("+" joins conditions; when the list runs out "free" is used):
//   any        no condition
//   free       no modal dialog is waiting for an answer and player 1 is not in social mode
//   dlg        a modal dialog (TryDialog kinds 0/0x30/0x40) has been up for 90 frames
//   idle       free, and player 1's sim has no queued action of the player's priority (100) or more
//   calm       idle, and player 1's sim has nothing queued or running at all (not even autonomy's)
//   soc        player 1 is in social mode
//   rmd        a reward moment dialog (RMDTarget) is up (SIMMY_RMD_AUTO=<frames> closes each with A)
//   bb         buy and build are not locked (the tutorial unlocks them)
//   t<hex>     an object of a "tutorial*" file runs tree <hex> (anywhere on its stack)
//   nt<hex>    ... does not
//   on<id>     object (person) <id> is on the lot (in a room: object data 0x1D >= 0)
//   g<guid>    an object of that GUID (hex) is in the world
//   e<guid>    an object of that GUID exists (also out of the world or in another's slot: the mail)
//   w<n>       the other conditions have held for n frames
//   x<cond>    once the others hold: the press is dropped unless <cond> holds too (a step the game
//              may already have gone past, e.g. "idle+xt1027")
//   k          a dropped press skips its SIMMY_INTERACT_NEAR entry too (an A press for an object)
//   !<cond>    <cond> does not hold; a|b  either holds (e.g. "dlg|bb+xdlg": a dialog the game may
//              already have answered itself)
// "[gate]" lines log each press held and played.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <deque>
#include <map>
#include <string>
#include <vector>

bool PlayerInSocialMode(int player);  // socialmode.cpp
extern int g_pcInteractNearPresses;   // directinteractor.cpp
bool PcModalDialogWaiting();          // objectsim.cpp (TryDialog)
int PcModalDialogCount();
bool H2DRewardMomentActive();         // rmdtarget.cpp

namespace simmy {
namespace {
struct Press {
  std::vector<PADStatus> frames;
  int onset = 0;  // the frame it started on
  int gap = 0;    // frames after the previous press's onset
};

bool Neutral(const PADStatus& p) {
  auto small = [](int v) { return v > -24 && v < 24; };
  return p.button == 0 && small(p.stickX) && small(p.stickY) && small(p.substickX) && small(p.substickY);
}

bool TutorialRuns(int tree) {
  if (!Globs_pObjectModule) return false;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) {
    if (!o->m_selector || std::strncmp(o->m_selector->m_fileName, "tutorial", 8) != 0 || !o->m_treeSim) continue;
    for (int i = 0; i < o->m_treeSim->GetStackSize(); ++i)
      if (StackElem* f = o->m_treeSim->GetNthElem(static_cast<short>(i)))
        if (f->GetTreeID() == tree) return true;
  }
  return false;
}

int SimQueueCount() {
  cXPerson* s = _globals.GetSelectedPerson(0);
  cXObject* o = s ? s->AsObject() : nullptr;
  auto* p = o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
  return p ? static_cast<int>(p->m_actionQueue.Count()) : 0;
}

bool SimHasPlayerAction() {
  cXPerson* s = _globals.GetSelectedPerson(0);
  cXObject* o = s ? s->AsObject() : nullptr;
  auto* p = o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
  if (!p) return false;
  for (unsigned int i = 0; i < p->m_actionQueue.Count(); ++i)
    if (p->m_actionQueue.At(i).GetPriority() >= 100) return true;
  return false;
}

int s_dialogSince = -1;

// One condition of a token; `wait` collects w<n>.
bool Holds(const std::string& c, int& wait) {
  if (c.empty() || c == "any" || c == "k") return true;
  const size_t bar = c.find('|');
  if (bar != std::string::npos) return Holds(c.substr(0, bar), wait) || Holds(c.substr(bar + 1), wait);
  if (c[0] == '!') return !Holds(c.substr(1), wait);
  const bool dialog = PcModalDialogWaiting();
  const bool social = PlayerInSocialMode(0);
  if (c == "free") return !dialog && !social;
  if (c == "dlg") return dialog && s_dialogSince >= 0 && _framecount - s_dialogSince >= 90;
  if (c == "idle") return !dialog && !social && !SimHasPlayerAction();
  if (c == "calm") return !dialog && !social && SimQueueCount() == 0;
  if (c == "soc") return social;
  if (c == "rmd") return H2DRewardMomentActive();
  if (c == "bb") return Globs_pObjectModule && !Globs_pObjectModule->IsBuyAndBuildDisabled();
  if (c[0] == 'w') {
    wait = std::atoi(c.c_str() + 1);
    return true;
  }
  if (c.compare(0, 2, "on") == 0) {
    cXObject* o = Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(std::atoi(c.c_str() + 2)) : nullptr;
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    return impl && impl->GetData(0x1d) >= 0 && impl->m_location[0] >= 0 && impl->m_location[1] >= 0;
  }
  if (c[0] == 'e' && Globs_pObjectModule) {
    const unsigned int guid = static_cast<unsigned int>(std::strtoul(c.c_str() + 1, nullptr, 16));
    for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
      if (o->m_selector && o->m_selector->GetGUID() == guid) return true;
    return false;
  }
  if (c[0] == 'g' && Globs_pObjectModule) {
    const unsigned int guid = static_cast<unsigned int>(std::strtoul(c.c_str() + 1, nullptr, 16));
    for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
      if (o->m_selector && o->m_selector->GetGUID() == guid && o->m_location[0] >= 0) return true;
    return false;
  }
  if (c[0] == 'u') {  // "u<lot>": location <lot> unlocked for the story family (GoalUnlock kind 2)
    GoalUnlock unlocks = FamilyGoalUnlock(1);
    const int lot = std::atoi(c.c_str() + 1);
    return unlocks.m_bits && lot >= 0 && lot < unlocks.GetUnlockCount(IGoalUnlock__UnlockType{2}) &&
           unlocks.IsUnlocked(IGoalUnlock__UnlockType{2}, lot);
  }
  if (c[0] == 't') return TutorialRuns(static_cast<int>(std::strtol(c.c_str() + 1, nullptr, 16)));
  if (c.compare(0, 2, "nt") == 0) return !TutorialRuns(static_cast<int>(std::strtol(c.c_str() + 2, nullptr, 16)));
  return true;
}
}  // namespace

// PC development aid: SIMMY_SIM_SPEED=<n>[@<house>] (-1 slow, 0 normal, -2 fast x5, -3 ultra x30:
// cSimulatorImpl's speeds) is set once each house (from the <house>-th one loaded, default the first)
// has been up 60 frames (long story stages in fewer frames).
void dev_sim_speed() {
  static const char* env = std::getenv("SIMMY_SIM_SPEED");
  static const char* at = env ? std::strchr(env, '@') : nullptr;
  static int houses = 0, since = -1;
  static bool applied = false;
  if (!env || !Globs_pSimulator) return;
  if (!_globals.m_house) {
    if (applied || since >= 0) applied = false, since = -1;
    return;
  }
  if (applied) return;
  if (since < 0) {
    since = _framecount;
    ++houses;
  }
  if (_framecount - since < 60) return;
  applied = true;
  if (houses < (at ? std::atoi(at + 1) : 1)) return;
  Globs_pSimulator->SetSpeed(SimSpeed{std::atoi(env)});
  std::printf("[gate] frame %d: simulator speed %d (SIMMY_SIM_SPEED, house load %d)\n", _framecount, std::atoi(env), houses);
}

// PC development aid: SIMMY_SIM_SPEED_JOB=<n>: the simulator speed n once player 1's sim has a job
// (person data 0x38: career runs that take the job offer at a calmer speed, then go through work days).
// With SIMMY_SIM_TRACE: "[career]" lines when a player's sim's career (person data 0x38), level (0x39),
// performance (0x3F) or missed days (0x18) change.
void dev_sim_speed_job() {
  static const char* env = std::getenv("SIMMY_SIM_SPEED_JOB");
  static const bool trace = std::getenv("SIMMY_SIM_TRACE") != nullptr;
  static bool applied = false;
  if (!Globs_pSimulator || !_globals.m_house) return;
  auto sim = [](int player) -> cXPersonImpl* {
    cXPerson* p = _globals.GetSelectedPerson(player);
    cXObject* o = p ? p->AsObject() : nullptr;
    return o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
  };
  if (trace && Globs_pObjectModule) {
    // Every person of the lot with a career (or a player's sim), by object id.
    static std::map<int, std::array<short, 4>> last;
    static const int kData[4] = {0x38, 0x39, 0x3f, 0x18};
    for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) {
      auto* p = o->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(o) : nullptr;
      if (!p || (p->GetPersonData(0x38) <= 0 && p != sim(0) && p != sim(1))) continue;
      std::array<short, 4> now;
      for (int k = 0; k < 4; ++k) now[k] = static_cast<short>(p->GetPersonData(kData[k]));
      auto it = last.find(p->m_id);
      if (it != last.end() && it->second == now) continue;
      last[p->m_id] = now;
      std::printf("[career] %s #%d: career %d level %d performance %d missed %d (%02d:%02d day %d); skills (data a c b 11 12 f 9)"
                  " %d %d %d %d %d %d %d\n", p->DebugName(), p->m_id, now[0], now[1], now[2], now[3], Globs_pSimulator->GetGlobal(0),
                  Globs_pSimulator->GetGlobal(5), Globs_pSimulator->GetGlobal(1), p->GetPersonData(0xa), p->GetPersonData(0xc),
                  p->GetPersonData(0xb), p->GetPersonData(0x11), p->GetPersonData(0x12), p->GetPersonData(0xf), p->GetPersonData(9));
    }
  }
  // SIMMY_CAREER_MOOD=<v>: a test harness for promotions: every sim with a career keeps its needs at v
  // at least (job performance is the mood when the carpool leaves).
  static const char* careerMood = std::getenv("SIMMY_CAREER_MOOD");
  if (careerMood && Globs_pObjectModule) {
    const float v = static_cast<float>(std::atof(careerMood));
    static const int kNeeds[] = {Motives::kEnergy, Motives::kComfort, Motives::kHunger, Motives::kHygiene,
                                 Motives::kBladder, Motives::kSocial, Motives::kFun};
    for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) {
      auto* p = o->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(o) : nullptr;
      if (!p || p->GetPersonData(0x38) <= 0) continue;
      for (int i : kNeeds)
        if (p->GetMotive(i) < v) p->SetMotive(i, v);
    }
  }
  // SIMMY_AUTONOMOUS_JOB=1: once player 1's sim has a job its interactor hands it to autonomy whenever it
  // has nothing queued (as after the idle time-out: SetIdlePlayerSimAutonomous), so it looks after itself
  // and takes the carpool over unattended work days.
  static const bool autoJob = std::getenv("SIMMY_AUTONOMOUS_JOB") != nullptr;
  if (autoJob) {
    cXPersonImpl* p = sim(0);
    InteractorModule__DirectInteractor* d = PlayerDirectInteractor(0);
    if (p && d && d->m_autonomous == 0 && p->GetPersonData(0x38) > 0 && p->CountActions(true) == 0)
      p->SetIdlePlayerSimAutonomous();
  }
  // SIMMY_PLAYER_WORK=1: the player sends player 1's sim to work: every 60 frames while it has a job and
  // nothing of the player's priority queued, "Go to Work" on a car (the portal's tree table; its check
  // passes once the carpool for this sim waits) is queued as from the pie menu, whatever the sim's mood
  // (GC: car_sg "auto to work" pushes only sims whose mood is above 0; vehicle_sg 200F "Interaction - Go
  // to Work TEST" has no mood rule, so a player can always send them).
  static const bool playerWork = std::getenv("SIMMY_PLAYER_WORK") != nullptr;
  if (playerWork && Globs_pObjectModule && _framecount % 60 == 0) {
    // player 1's family member with a job (the player switches to whoever the carpool is for)
    cXPersonImpl* p = sim(0);
    if (!p || p->GetPersonData(0x38) <= 0)
      for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) {
        auto* q = o->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(o) : nullptr;
        if (q && q->GetPersonData(0x38) > 0 && q->m_location[0] >= 0 && q->IsSelectableByPlayer(0)) {
          p = q;
          break;
        }
      }
    bool queued = false;
    // (the action running, e.g. sleeping, does not count: the player's choice wakes the sim)
    if (p)
      for (unsigned int i = p->m_hasCurrentAction ? 1u : 0u; i < p->m_actionQueue.Count() && !queued; ++i)
        queued = p->m_actionQueue.At(i).GetPriority() >= 100;
    if (p && p->m_hasCurrentAction && p->m_actionQueue.Count()) {  // already going (the car's own push)
      cXObject* on = p->m_actionQueue.At(0).GetStackObject();
      if (on && std::strstr(on->GetObjectImplementation()->DebugName(), "portal_car")) queued = true;
    }
    static const bool verbose = std::getenv("SIMMY_PLAYER_WORK")[0] == '2';
    bool sent = false;
    int cars = 0;
    if (p && !queued && p->GetPersonData(0x38) > 0 && p->GetData(0x22) == 0 && p->m_location[0] >= 0)
      for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
        if (o->m_selector && o->m_location[0] >= 0 && std::strstr(o->DebugName(), "portal_car")) {
          ++cars;
          if (!PushInteraction(p, o, "Go to Work")) continue;
          std::printf("[gate] frame %d: player 1 sends %s to work (SIMMY_PLAYER_WORK, mood %.1f)\n", _framecount,
                      p->DebugName(), static_cast<double>(p->GetMotive(Motives::kMood)));
          sent = true;
          break;
        }
    if (verbose && p && !sent && _framecount % 600 == 0 && Globs_pSimulator)
      std::printf("[gate] player work: not sent at %02d:%02d (queued %d, job %d, data 0x22 %d, at %d, cars %d)\n",
                  Globs_pSimulator->GetGlobal(0), Globs_pSimulator->GetGlobal(5), queued ? 1 : 0, p->GetPersonData(0x38),
                  p->GetData(0x22), p->m_location[0], cars);
  }
  if (!env || applied) return;
  cXPersonImpl* person = sim(0);
  if (!person || person->GetPersonData(0x38) <= 0) return;
  applied = true;
  Globs_pSimulator->SetSpeed(SimSpeed{std::atoi(env)});
  std::printf("[gate] frame %d: simulator speed %d (SIMMY_SIM_SPEED_JOB: career %d level %d)\n", _framecount, std::atoi(env),
              person->GetPersonData(0x38), person->GetPersonData(0x39));
}

void dev_pad_gate_impl(PADStatus* pads);

// PC development aid: SIMMY_DIALOG_AUTO=<frames>: a modal dialog that has waited that long is answered
// with A on pad 1 (its first button), when the pad is otherwise neutral (long unattended story runs).
void dev_dialog_auto(PADStatus* pads) {
  static const char* env = std::getenv("SIMMY_DIALOG_AUTO");
  if (!env) return;
  static int since = -1, pressing = 0, shown = -1;
  const bool modal = PcModalDialogWaiting();
  if (!modal) {
    since = -1;
    pressing = 0;
    return;
  }
  if (since < 0 || PcModalDialogCount() != shown) since = _framecount, shown = PcModalDialogCount();
  if (pressing > 0) {
    if (--pressing > 2) pads[0].button |= PAD_BUTTON_A;
    if (pressing == 0) since = _framecount;  // not answered: try again later
    return;
  }
  if (_framecount - since >= std::atoi(env) && Neutral(pads[0])) {
    pressing = 6;
    pads[0].button |= PAD_BUTTON_A;
    std::printf("[gate] frame %d: modal dialog answered (SIMMY_DIALOG_AUTO)\n", _framecount);
  }
}

// PC development aid: SIMMY_RMD_AUTO=<frames>: a reward moment dialog up that long is closed with A on
// pad 1 (as a player would; unattended story runs: the scripted presses are gated "!rmd", so without
// this each one only closes by its 60 s time-out).
void dev_rmd_auto(PADStatus* pads) {
  static const char* env = std::getenv("SIMMY_RMD_AUTO");
  if (!env) return;
  static int since = -1, pressing = 0;
  if (!H2DRewardMomentActive()) {
    since = -1;
    pressing = 0;
    return;
  }
  if (since < 0) since = _framecount;
  if (pressing > 0) {
    if (--pressing > 2) pads[0].button |= PAD_BUTTON_A;
    if (pressing == 0) since = _framecount;  // still up (not shown yet): again later
    return;
  }
  if (_framecount - since >= std::atoi(env) && Neutral(pads[0])) {
    pressing = 6;
    pads[0].button |= PAD_BUTTON_A;
    std::printf("[gate] frame %d: reward moment answered (SIMMY_RMD_AUTO)\n", _framecount);
  }
}

void dev_pad_gate(PADStatus* pads) {
  dev_pad_gate_impl(pads);
  dev_dialog_auto(pads);
  dev_rmd_auto(pads);
}

void dev_pad_gate_impl(PADStatus* pads) {
  dev_sim_speed();
  dev_sim_speed_job();
  static const char* env = std::getenv("SIMMY_PAD_GATE");
  if (!env) return;
  static std::vector<std::string> tokens;
  static bool parsed = false;
  if (!parsed) {
    parsed = true;
    for (const char* p = env; *p;) {
      const char* end = std::strchr(p, ',');
      tokens.emplace_back(p, end ? end : p + std::strlen(p));
      if (!end) break;
      p = end + 1;
    }
  }
  static std::deque<Press> queue;
  static bool recording = false;
  static int lastOnset = -1;
  static size_t index = 0;          // the next press to play
  static int playing = -1;          // frame within the press being played
  static Press current;
  static int lastPlayed = -100000;  // the onset (frame) of the last press played
  static int holdsSince = -1;

  // When the modal dialog now up was shown (a new one right after an answered one counts afresh).
  const bool modal = PcModalDialogWaiting();
  static int shown = 0;
  if (modal && (s_dialogSince < 0 || PcModalDialogCount() != shown)) s_dialogSince = _framecount;
  shown = PcModalDialogCount();
  if (!modal) s_dialogSince = -1;

  PADStatus& pad = pads[0];
  // Before the first house has been up 120 frames (SIMMY_PAD_SEQ's "house" step; the menu's last press
  // reaches live mode before that): the pad as scripted.
  static bool active = false;
  static int houseSince = -1;
  if (_globals.m_house && houseSince < 0) houseSince = _framecount;
  if (!active && houseSince >= 0 && _framecount - houseSince >= 120 && Neutral(pad)) active = true;
  if (!active) return;
  // Record what the script pressed this frame.
  const bool neutral = Neutral(pad);
  if (!neutral) {
    if (!recording) {
      recording = true;
      Press p;
      p.onset = _framecount;
      p.gap = lastOnset < 0 ? 0 : _framecount - lastOnset;
      lastOnset = _framecount;
      queue.push_back(p);
    }
    queue.back().frames.push_back(pad);
  } else if (recording) {
    recording = false;
    const size_t n = index + queue.size();
    std::printf("[gate] frame %d: press %zu (buttons %04x) held for \"%s\"\n", _framecount, n - 1,
                queue.back().frames.empty() ? 0 : queue.back().frames[0].button,
                n - 1 < tokens.size() ? tokens[n - 1].c_str() : "free");
  }
  const PADStatus blank = [&] {
    PADStatus b = pad;
    b.button = 0;
    b.stickX = b.stickY = b.substickX = b.substickY = 0;
    b.triggerLeft = b.triggerRight = b.analogA = b.analogB = 0;
    return b;
  }();
  pad = blank;
  // Play back.
  if (playing >= 0) {
    if (playing < static_cast<int>(current.frames.size())) {
      PADStatus s = current.frames[static_cast<size_t>(playing++)];
      s.err = blank.err;
      pad = s;
      return;
    }
    playing = -1;  // one neutral frame between presses
    return;
  }
  // A press is ready once it is fully recorded.
  if (queue.empty() || (recording && queue.size() == 1)) return;
  const Press& next = queue.front();
  const std::string tok = index < tokens.size() ? tokens[index] : "free";
  int wait = 0;
  bool ok = true, keep = true;
  for (size_t at = 0; at <= tok.size();) {
    const size_t plus = tok.find('+', at);
    const std::string c = tok.substr(at, plus == std::string::npos ? std::string::npos : plus - at);
    if (!c.empty() && c[0] == 'x') {
      int unused = 0;
      if (!Holds(c.substr(1), unused)) keep = false;
    } else if (!Holds(c, wait)) {
      ok = false;
    }
    at = plus == std::string::npos ? tok.size() + 1 : plus + 1;
  }
  if (ok && !keep) {
    std::printf("[gate] frame %d: press %zu dropped (\"%s\")\n", _framecount, index, tok.c_str());
    if (("+" + tok + "+").find("+k+") != std::string::npos) ++g_pcInteractNearPresses;
    queue.pop_front();
    ++index;
    holdsSince = -1;
    return;
  }
  if (!ok) {
    holdsSince = -1;
    // Held long: which conditions fail (once per press).
    static size_t reported = static_cast<size_t>(-1);
    if (reported != index && _framecount - lastPlayed > 3000) {
      reported = index;
      std::string why;
      for (size_t at = 0; at <= tok.size();) {
        const size_t plus = tok.find('+', at);
        const std::string c = tok.substr(at, plus == std::string::npos ? std::string::npos : plus - at);
        int unused = 0;
        if (c.empty() || c[0] == 'x' || c[0] == 'w') {
        } else if (!Holds(c, unused)) {
          why += " " + c;
        }
        at = plus == std::string::npos ? tok.size() + 1 : plus + 1;
      }
      std::printf("[gate] frame %d: press %zu still held (\"%s\"): not%s\n", _framecount, index, tok.c_str(), why.c_str());
      // ... and where the tutorial is (its stack's trees).
      for (cXObjectImpl* o = Globs_pObjectModule ? Globs_pObjectModule->m_first : nullptr; o; o = o->m_nextObject) {
        if (!o->m_selector || std::strncmp(o->m_selector->m_fileName, "tutorial", 8) != 0 || !o->m_treeSim) continue;
        std::printf("[gate]   %s:", o->DebugName());
        for (int i = 0; i < o->m_treeSim->GetStackSize(); ++i)
          if (StackElem* f = o->m_treeSim->GetNthElem(static_cast<short>(i))) std::printf(" %04x:%d", f->GetTreeID(), f->m_node);
        std::printf("\n");
      }
    }
    return;
  }
  if (holdsSince < 0) holdsSince = _framecount;
  const int gap = next.gap < 30 ? 30 : next.gap;
  if (_framecount - holdsSince < wait || _framecount - lastPlayed < gap) return;
  current = next;
  queue.pop_front();
  std::printf("[gate] frame %d: press %zu (buttons %04x) played (\"%s\", recorded at %d)\n", _framecount, index,
              current.frames.empty() ? 0 : current.frames[0].button, tok.c_str(), current.onset);
  ++index;
  holdsSince = -1;
  lastPlayed = _framecount;
  playing = 0;
  PADStatus s = current.frames[static_cast<size_t>(playing++)];
  s.err = blank.err;
  pad = s;
}
}  // namespace simmy
