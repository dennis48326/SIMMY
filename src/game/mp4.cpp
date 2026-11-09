// PC mod: 4 players (include/simmy/mp4.hpp, docs/multiplayer.md). Not a port of GameCube code: the
// GameCube game has two players. This file adds players 3 and 4 to a two-player Free Play game.
//
// Joining: with players=3 or 4 (simmy.ini, SIMMY_PLAYERS), in a two-player Free Play game on a lot,
// Start on a connected controller that no player has yet takes the next free extra player slot
// (player 3, then 4) and the first sim of the household that no player has (the GameCube's way of
// taking a family member: EGlobal's SelectableMember checks, here over the player family). The
// controller is mapped to that player (EControllerManager::MapPlayerToController, which has four
// slots on the GameCube too). Without a free sim the player cannot join (the log says why; player 1's
// window shows a note). Story mode, one-player games and the build/buy screens keep two players.
//
// Playing: the extra player's slot runs the same direct-control interactor, camera (ESimsCam with its
// own SimsCameraParameters) and camera director as players 1 and 2 (ELiveMode::Update and DrawMain
// loop over Mp4PlayerSlots), a plumb bob in the player's colour, and their own window. The GameCube's
// HUD (Flash movies) only has two players' layouts, so an extra player's HUD is drawn here with the
// game's font and 2D rectangles into their window: who they play, the sim's current action and
// motives, the action menu (A: the interactions of the objects in front of the sim, from
// CollectInteractionsForObject, started as the GC menu's choice is: StartMenuInteraction), and the
// drop-out prompt. Extra players have no classic (cursor) control, build/buy, social-mode menu or
// pause menu of their own; the pause menu players 1 and 2 open pauses everyone, as on the GC.
//
// Dropping out: Start, then A ("drop out?"), or closing the player's window. The sim goes back to
// autonomy. Leaving the lot (quitting to the main menu, a lot change) drops every extra player;
// nothing about them is saved (the GameCube save has two players).
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/mp4.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern ENgcRenderer _ngcRenderer;
void ShutdownDirectInteractorOf(int player);  // directinteractor.cpp
cXPerson* PcFirstFreeFamilyMember(int player);  // familyselect.cpp

namespace {
// EController buttons (ENgcController::GetButtons).
constexpr unsigned int kBtnA = 0x40, kBtnB = 0x80, kBtnStart = 0x800, kBtnUp = 0x1000, kBtnDown = 0x4000;

struct MenuRow {
  std::string label;
  MenuInteraction interaction;
  bool disabled = false;
};

struct Extra {
  bool joined = false;
  int pad = -1;
  bool menu = false;
  int menuClosedFrame = -1;  // the frame the menu closed (its press is not the interactor's)
  std::vector<MenuRow> rows;
  int cursor = 0;
  bool prompt = false;  // "drop out?"
  std::string note;
  int noteUntil = 0;
  int startUntil = 0;  // SIMMY_MP4_START: kept on its tile until then (an action under way may move it)
  float startX = 0.0f, startY = 0.0f;
};
Extra g_x[kMaxPlayers];  // [2] and [3]
std::string g_note;      // for player 1's window: a join that could not happen
int g_noteUntil = 0;

// Players 3 and 4: red and yellow, at the GC colours' brightness (PlumbBobParms m_colorScale holds
// player 1's green 0.51/1.99/0.72 and player 2's blue 0.51/1.22/1.99).
const float kColor[kMaxPlayers][3] = {{0.51f, 1.99f, 0.72f}, {0.51f, 1.22f, 1.99f}, {1.99f, 0.55f, 0.45f}, {1.99f, 1.75f, 0.35f}};

std::string Narrow(const std::u16string& s) {
  std::string out;
  bool tag = false;
  for (char16_t c : s) {
    if (c == u'<') tag = true;  // Flash colour tags (ColorStringForFlash)
    if (!tag) out += c == u'_' ? ' ' : c < 0x80 ? static_cast<char>(c) : '?';
    if (c == u'>') tag = false;
  }
  return out;
}

cXPersonImpl* PersonOf(cXPerson* p) {
  cXObject* o = p ? p->AsObject() : nullptr;
  return o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
}

bool InModule(cXObjectImpl* object) {
  if (!object || !Globs_pObjectModule) return false;
  for (int id = 1; id <= Globs_pObjectModule->GetNumObjects(); ++id) {
    cXObject* x = Globs_pObjectModule->GetObjectFromID(static_cast<short>(id));
    if (x && x->GetObjectImplementation() == object) return true;
  }
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
    if (o == object) return true;
  return false;
}

void Note(int player, const std::string& text) {
  std::printf("[mp4] player %d: %s\n", player + 1, text.c_str());
  if (player >= kGcPlayers && g_x[player].joined) {
    g_x[player].note = text;
    g_x[player].noteUntil = _framecount + 240;
  } else {
    g_note = "Player " + std::to_string(player + 1) + ": " + text;
    g_noteUntil = _framecount + 240;
  }
}

// Why no extra player can join now (empty: they can).
std::string JoinRefusal() {
  if (simmy::settings_max_players() < 3) return "players= is below 3";
  if (!_globals.m_house || !Globs_pObjectModule || !Globs_pNeighborhood) return "no lot";
  if (!_gd.m_fieldE8) return "story mode has two players at most (Free Play only)";  // DAT_80475C6C: free play
  if (!_gd.m_fieldEC) return "start a two-player Free Play game first";              // DAT_80475C70
  if (simmy::split_mode() != simmy::SplitMode::Windows && !simmy::platform_headless())
    return "extra players need split=windows (one window per player)";
  if (_gd.GetBuildBuyPlayer() != -1) return "build/buy mode is open";
  if (_globals.m_fieldAC || _globals.m_fieldB0) return "the lot's intro camera is running";
  return {};
}

bool PadConnected(int c) {
  EController* pad = _pCtrlMan ? _pCtrlMan->GetController(static_cast<unsigned int>(c)) : nullptr;
  return pad && (pad->GetStatus() & 5) == 5;
}

// The pad `c` is some player's (players 1 and 2: their mapping; the extra players: theirs).
bool PadTaken(int c) {
  for (int p = 0; p < kGcPlayers; ++p)
    if (_pCtrlMan->GetPlayerMapped(static_cast<unsigned int>(p)) && _pCtrlMan->GetPlayerControllerIndex(static_cast<unsigned int>(p)) == c)
      return true;
  for (int p = kGcPlayers; p < kMaxPlayers; ++p)
    if (g_x[p].joined && g_x[p].pad == c) return true;
  return false;
}

void Join(int player, int pad) {
  cXPerson* person = PcFirstFreeFamilyMember(player);
  if (!person) {
    Note(player, "no free sim in the household (every sim has a player)");
    return;
  }
  Extra& x = g_x[player];
  x = Extra{};
  x.joined = true;
  x.pad = pad;
  _pCtrlMan->MapPlayerToController(static_cast<unsigned int>(player), static_cast<unsigned int>(pad));
  // The player's camera director, camera (Player::Reset without the GC HUD's pie menu) and plumb bob.
  if (!_globals.m_cameraDirectors[player]) _globals.m_cameraDirectors[player] = new CameraDirector();
  _globals.m_cameraDirectors[player]->Reset();
  Player& pl = _gd.m_players[player];
  pl.m_index = player;
  if (!pl.m_cam) pl.m_cam = new ESimsCam(player);
  pl.m_cam->Init();
  if (!PlumbBob::s_pPlumbBob[player]) {
    PlumbBob::s_pPlumbBob[player] = new PlumbBob;
    PlumbBob::s_pPlumbBob[player]->SetPlayerColor(player);
  }
  PlumbBob::s_pPlumbBob[player]->Startup();
  // PC test aid: SIMMY_MP4_START="3:x,y,4:x,y" puts the joining player's sim on that tile first.
  if (const char* start = std::getenv("SIMMY_MP4_START"))
    for (const char* p = start; *p;) {
      int who = 0;
      float tx = 0.0f, ty = 0.0f;
      if (std::sscanf(p, "%d:%f,%f", &who, &tx, &ty) == 3 && who == player + 1) {
        x.startUntil = _framecount + 60, x.startX = tx, x.startY = ty;
        std::printf("[mp4] player %d's sim goes to %.1f,%.1f (SIMMY_MP4_START)\n", player + 1, tx, ty);
      }
      const char* next = std::strchr(p, ',');
      if (next) next = std::strchr(next + 1, ',');  // past "x,y"
      if (!next) break;
      p = next + 1;
    }
  _globals.SetSelectedPerson(player, person);
  _globals.m_usedSimInteractor[player] = 0;
  // The camera's tuning as player 1's (ESimsCam::s_params), then placed behind the sim (ESimsCam::Reset,
  // as live mode's start does for players 1 and 2).
  ESimsCam::s_params[player] = ESimsCam::s_params[0];
  pl.m_cam->Reset();
  pl.m_cam->CenterOnSelectedSim();
  static const bool trace = std::getenv("SIMMY_MP4_TRACE") != nullptr;  // PC dev trace: the lot's objects
  if (trace)
    for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
      std::printf("[mp4] object %d %s at %d,%d\n", o->m_id, o->DebugName(), o->GetCTilePt().GetX(), o->GetCTilePt().GetY());
  cXPersonImpl* impl = PersonOf(person);
  std::printf("[mp4] player %d joins on pad %d with %s (players: %d)\n", player + 1, pad + 1, impl ? impl->DebugName() : "?",
              Mp4PlayerSlots());
  x.note = "Player " + std::to_string(player + 1) + " joined";
  x.noteUntil = _framecount + 180;
}

void Drop(int player, const char* why) {
  Extra& x = g_x[player];
  if (!x.joined) return;
  cXPerson* person = _globals.GetSelectedPerson(player);
  ShutdownDirectInteractorOf(player);
  if (person) {
    // The sim is nobody's any more: autonomy may take it (EGlobal::SetSelectedPerson's release, which
    // its null case skips).
    if (SAnimator2* anim = person->GetSAnimator()) anim->SetPlayerControl(false);
  }
  _globals.m_selectedPerson[player] = nullptr;
  if (PlumbBob* bob = PlumbBob::s_pPlumbBob[player]) {
    bob->Shutdown();
    delete bob;
    PlumbBob::s_pPlumbBob[player] = nullptr;
  }
  if (CameraDirector* d = _globals.m_cameraDirectors[player]) d->Reset();
  _pCtrlMan->UnmapPlayer(static_cast<unsigned int>(player));
  x = Extra{};
  std::printf("[mp4] player %d drops out (%s; players: %d)\n", player + 1, why, Mp4PlayerSlots());
}

void BuildMenu(int player, const std::vector<cXObjectImpl*>& objects) {
  Extra& x = g_x[player];
  x.rows.clear();
  x.cursor = 0;
  for (cXObjectImpl* o : objects) {
    MenuInteractionList list;
    CollectInteractionsForObject(o, list, player, 10000);  // as FlashPiMenu::AddObjectInteractionsToMenu
    const std::string object = Narrow(GetObjectMenuName(o));
    for (const MenuInteraction& m : list) {
      MenuRow row;
      std::string name = Narrow(m.name);
      // Social mode (the person's "Socialize") needs the GC social menu, which has two players' screens
      // only: an extra player's sim would wait in it for good. Not offered to them.
      if (name == "Socialize" && o->GetType() == 2) continue;
      for (size_t at = name.find('/'); at != std::string::npos; at = name.find('/', at + 3)) name.replace(at, 1, " > ");
      row.label = objects.size() > 1 && !object.empty() ? object + ": " + name : name;
      row.interaction = m;
      row.disabled = (m.flags >> 0xb & 1) != 0;
      x.rows.push_back(row);
    }
  }
}

void CloseMenu(int player) {
  Extra& x = g_x[player];
  if (!x.menu) return;
  x.menu = false;
  x.rows.clear();
  x.menuClosedFrame = _framecount;
}

void Choose(int player) {
  Extra& x = g_x[player];
  if (x.cursor < 0 || x.cursor >= static_cast<int>(x.rows.size())) return;
  const MenuRow& row = x.rows[static_cast<size_t>(x.cursor)];
  cXPersonImpl* me = PersonOf(_globals.GetSelectedPerson(player));
  int r = 0;
  // As FlashPiMenu::ChooseAction: the object and the person must still be there.
  if (!row.disabled && me && row.interaction.person == me && InModule(row.interaction.object)) r = StartMenuInteraction(row.interaction);
  UIAUDIO::Instance()->PlaySound(r ? 0x374bdcea : 0x6e2778b7);
  std::printf("[mp4] player %d chooses \"%s\": %s\n", player + 1, row.label.c_str(), r ? "queued" : "refused");
  CloseMenu(player);
}

void UpdatePlayer(int player) {
  Extra& x = g_x[player];
  if (_framecount < x.startUntil)  // (test aid, SIMMY_MP4_START)
    if (cXPersonImpl* impl = PersonOf(_globals.GetSelectedPerson(player))) {
      const CTilePt t = impl->GetCTilePt();
      if (t.GetX() != static_cast<int>(x.startX) || t.GetY() != static_cast<int>(x.startY)) {
        impl->CancelAllActions();
        impl->SetLocation(x.startX + 0.5f, x.startY + 0.5f, impl->m_level ? impl->m_level : 1, impl->GetData(1) & 7);
        if (impl->m_animator) impl->m_animator->SnapToGrid();
      }
    }
  if (simmy::split_window_take_close_request(player)) return Drop(player, "the window was closed");
  if (!_globals.GetSelectedPerson(player)) return Drop(player, "the sim left the lot");
  if (!PadConnected(x.pad)) return Drop(player, "the controller was unplugged");
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(x.pad));
  const unsigned int pressed = pad->GetBtnPressedMask(0);
  if (x.prompt) {
    if (pressed & kBtnA) return Drop(player, "the player chose to");
    if (pressed & (kBtnB | kBtnStart)) x.prompt = false;
    return;
  }
  if (pressed & kBtnStart) {
    CloseMenu(player);
    x.prompt = true;
    return;
  }
  if (!x.menu) return;
  // The sim changed (X / Y, or the sim was taken away): the menu is for the old one.
  cXPersonImpl* me = PersonOf(_globals.GetSelectedPerson(player));
  if (!x.rows.empty() && x.rows[0].interaction.person != me) return CloseMenu(player);
  const int n = static_cast<int>(x.rows.size());
  if (pressed & kBtnB) return CloseMenu(player);
  if (n && (pressed & kBtnDown)) x.cursor = (x.cursor + 1) % n;
  if (n && (pressed & kBtnUp)) x.cursor = (x.cursor + n - 1) % n;
  if (pressed & kBtnA) Choose(player);
}
}  // namespace

bool Mp4Joined(int player) { return player >= kGcPlayers && player < kMaxPlayers && g_x[player].joined; }

bool Mp4AnyJoined() {
  for (int p = kGcPlayers; p < kMaxPlayers; ++p)
    if (g_x[p].joined) return true;
  return false;
}

int Mp4PlayerSlots() {
  int slots = _gd.m_fieldEC ? 2 : 1;
  for (int p = kGcPlayers; p < kMaxPlayers; ++p)
    if (g_x[p].joined) slots = p + 1;
  return slots;
}

const float* Mp4PlayerColor(int player) { return kColor[player >= 0 && player < kMaxPlayers ? player : 0]; }

bool Mp4MenuOpen(int player) {
  return Mp4Joined(player) && (g_x[player].menu || g_x[player].prompt || g_x[player].menuClosedFrame == _framecount);
}

void Mp4CloseMenu(int player) {
  if (Mp4Joined(player)) CloseMenu(player);
}

bool Mp4ShowActionMenu(int player, const std::vector<cXObjectImpl*>& objects) {
  if (!Mp4Joined(player)) return false;
  BuildMenu(player, objects);
  Extra& x = g_x[player];
  x.menu = !x.rows.empty();
  if (x.menu) {
    std::printf("[mp4] player %d's action menu: %d interactions:", player + 1, static_cast<int>(x.rows.size()));
    for (const MenuRow& r : x.rows) std::printf(" \"%s\"%s", r.label.c_str(), r.disabled ? " (disabled)" : "");
    std::printf("\n");
    // PC test aid: SIMMY_MP4_PICK="3:Snack|Eat,4:Use" puts the cursor on the first row with one of those
    // texts when that player's menu opens (the script then presses A).
    if (const char* pick = std::getenv("SIMMY_MP4_PICK"))
      for (const char* p = pick; *p;) {
        const char* end = std::strchr(p, ',');
        const std::string item(p, end ? end : p + std::strlen(p));
        if (item.size() > 2 && item[0] - '1' == player && item[1] == ':') {
          std::string alts = item.substr(2) + "|";
          bool found = false;
          for (size_t a = 0, b; !found && (b = alts.find('|', a)) != std::string::npos; a = b + 1) {
            const std::string want = alts.substr(a, b - a);
            for (size_t r = 0; r < x.rows.size() && !found && !want.empty(); ++r)
              if (x.rows[r].label.find(want) != std::string::npos) x.cursor = static_cast<int>(r), found = true;
          }
        }
        if (!end) break;
        p = end + 1;
      }
  }
  return x.menu;
}

void Mp4Update() {
  if (simmy::settings_max_players() < 3 && !Mp4AnyJoined()) return;
  // The extra players' views need their windows: another co-op display (F1 options) drops them.
  if (Mp4AnyJoined() && simmy::split_mode() != simmy::SplitMode::Windows && !simmy::platform_headless())
    Mp4DropAll("the co-op display is no longer split=windows");
  for (int p = kGcPlayers; p < kMaxPlayers; ++p)
    if (g_x[p].joined) UpdatePlayer(p);
  // A sim is one player's (EGlobal::SimInUseByOtherPlayer); SIMMY_MP4_TRACE lists them every 600 frames.
  static const bool traceSims = std::getenv("SIMMY_MP4_TRACE") != nullptr;
  if (Mp4AnyJoined() && (traceSims ? _framecount % 600 == 0 : _framecount % 60 == 0)) {
    bool shared = false;
    for (int a = 0; a < kMaxPlayers; ++a)
      for (int b = a + 1; b < kMaxPlayers; ++b)
        if (_globals.m_selectedPerson[a] && _globals.m_selectedPerson[a] == _globals.m_selectedPerson[b]) shared = true;
    if (shared || traceSims) {
      std::printf("[mp4] %ssims:", shared ? "WARNING: two players share a sim; " : "");
      for (int p = 0; p < kMaxPlayers; ++p) {
        cXPersonImpl* me = PersonOf(_globals.m_selectedPerson[p]);
        std::printf(" p%d %d", p + 1, me ? me->m_id : 0);
      }
      std::printf("\n");
    }
  }
  // A pause (the pause menu, a dialog) holds the game for everyone: no joining meanwhile.
  if (H2DTarget::m_nPauseRefCount != 0 || !_pCtrlMan) return;
  const int most = std::min(simmy::settings_max_players(), kMaxPlayers);
  static const bool trace = std::getenv("SIMMY_MP4_TRACE") != nullptr;  // PC dev trace
  for (int c = 0; c < 4; ++c) {
    if (trace)
      if (EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(c)))
        if (pad->GetBtnPressedMask(~0u) & kBtnStart)
          std::printf("[mp4] f%d pad %d Start (connected %d, taken %d, filtered %d; players 1, 2 on %d:%d %d:%d)\n", _framecount, c + 1,
                      PadConnected(c) ? 1 : 0, PadTaken(c) ? 1 : 0, (pad->GetBtnPressedMask(0) & kBtnStart) ? 0 : 1,
                      _pCtrlMan->GetPlayerMapped(0), _pCtrlMan->GetPlayerControllerIndex(0), _pCtrlMan->GetPlayerMapped(1),
                      _pCtrlMan->GetPlayerControllerIndex(1));
    if (!PadConnected(c) || PadTaken(c)) continue;
    EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(c));
    if (!(pad->GetBtnPressedMask(0) & kBtnStart)) continue;
    int slot = -1;
    for (int p = kGcPlayers; p < most && slot < 0; ++p)
      if (!g_x[p].joined) slot = p;
    if (slot < 0) {
      std::printf("[mp4] pad %d: every player slot is taken (players=%d)\n", c + 1, simmy::settings_max_players());
      continue;
    }
    const std::string why = JoinRefusal();
    if (!why.empty()) {
      Note(slot, "cannot join: " + why);
      continue;
    }
    Join(slot, c);
  }
}

void Mp4DropAll(const char* why) {
  for (int p = kGcPlayers; p < kMaxPlayers; ++p) Drop(p, why);
  simmy::split_set_window_count(2);
}

// ---------------------------------------------------------------------------------------------------
// The extra players' HUD (their windows; 640x480 stage units, normalised for the font and rectangles)

namespace {
ERFont* Font() {
  static ERFont* font = static_cast<ERFont*>(_fontman.AddRef(0x67f664fbu, nullptr, 0));  // the game font
  return font && font->m_size ? font : nullptr;
}

void Box(ERC* rc, float x0, float y0, float x1, float y1, const EVec4& color) {
  EShader* plain = static_cast<ENgcGraphics*>(_pGfx)->m_defaultShader;
  if (!plain) return;
  plain->Select(rc, 0xffffffffu);
  rc->SetBlendMode(0, 1, 0, 1, 1.0f, 0);  // (src - dst) * srcAlpha + dst
  // The game's textures are stored bottom row first, so v runs up the screen.
  EVec2 p0{x0 / 640.0f, y0 / 480.0f}, p1{x1 / 640.0f, y1 / 480.0f}, uv0{0, 1}, uv1{1, 0};
  EVec4 c = color;
  rc->Rect(p0, p1, uv0, uv1, c, 0.0f);
}

void Text(ERC* rc, const std::string& s, float x, float y, const EVec4& color, float size = 20.0f,
          int align = EFontAlignX::Left) {
  ERFont* font = Font();
  if (!font || s.empty()) return;
  font->SetSize(size, 1.0f, true);
  font->Select(rc);
  EVec4 c = color;
  font->SetColor(c);
  EVec2 pos{x / 640.0f, y / 480.0f};
  std::string copy = s;
  font->DrawDs(rc, copy.data(), pos, EFontAlignX{align}, EFontAlignY::Top, nullptr, 2.0f, 0.8f, true);
}

void DrawPlayer(ERC* rc, int player) {
  const Extra& x = g_x[player];
  const float* k = kColor[player];
  const EVec4 colour{std::min(1.0f, k[0] * 0.5f), std::min(1.0f, k[1] * 0.5f), std::min(1.0f, k[2] * 0.5f), 1.0f};
  const EVec4 white{1, 1, 1, 1}, grey{0.7f, 0.7f, 0.7f, 1}, panel{0.04f, 0.06f, 0.25f, 0.72f}, sel{0.25f, 0.69f, 0.33f, 0.9f};
  cXPerson* person = _globals.GetSelectedPerson(player);
  cXPersonImpl* me = PersonOf(person);
  // Who: "Player 3 - <sim>" and the sim's current action.
  // (Bottom left: the GC's hint and dialog boxes use the top of the screen.)
  std::string who = "Player " + std::to_string(player + 1);
  if (me && me->m_neighbor && me->m_neighbor->m_selector) who += " - " + Narrow(me->m_neighbor->m_selector->GetUserName());
  Box(rc, 8, 412, 300, 470, panel);
  Text(rc, who, 16, 416, colour, 18.0f);
  if (me) {
    std::string doing = "Idle";
    if (me->m_actionQueue.Count()) doing = Narrow(me->m_actionQueue.At(0).GetName());
    if (me->m_actionQueue.Count() > 1) doing += " (+" + std::to_string(me->m_actionQueue.Count() - 1) + ")";
    Text(rc, doing, 16, 442, white, 15.0f);
  }
  // Motives: the GC HUD's six bars' order (bladder, hygiene, hunger, fun, energy) plus comfort, social, room.
  if (person) {
    static const struct {
      const char* name;
      int index;
    } kMotives[] = {{"Hunger", 7}, {"Comfort", 6}, {"Hygiene", 8}, {"Bladder", 9},
                    {"Energy", 5}, {"Fun", 0xf},    {"Social", 0xe}, {"Room", 0xd}};
    const float x0 = 478.0f, y0 = 312.0f, rowH = 19.0f;
    Box(rc, x0 - 8, y0 - 6, 632, y0 + 8 * rowH + 2, panel);
    for (int i = 0; i < 8; ++i) {
      const float v = static_cast<float>(person->GetMotive(kMotives[i].index));
      const float t = std::clamp((v + 100.0f) / 200.0f, 0.0f, 1.0f);
      const float y = y0 + rowH * static_cast<float>(i);
      Text(rc, kMotives[i].name, x0, y, white, 14.0f);
      Box(rc, x0 + 70, y + 4, x0 + 146, y + 13, EVec4{0.1f, 0.1f, 0.1f, 0.9f});
      const EVec4 bar{t < 0.5f ? 0.9f : 1.8f * (1.0f - t), t < 0.5f ? 1.8f * t : 0.85f, 0.1f, 1.0f};
      Box(rc, x0 + 70, y + 4, x0 + 70 + 76.0f * t, y + 13, bar);
    }
  }
  // The action menu: up to nine rows around the cursor.
  if (x.menu && !x.rows.empty()) {
    const int n = static_cast<int>(x.rows.size()), shown = std::min(n, 9);
    const int first = std::clamp(x.cursor - shown / 2, 0, n - shown);
    const float x0 = 130.0f, y0 = 140.0f, rowH = 22.0f;
    Box(rc, x0 - 10, y0 - 30, 510, y0 + rowH * static_cast<float>(shown) + 8, panel);
    Text(rc, "Actions  (A: do it, B: back)", x0, y0 - 26, colour, 15.0f);
    for (int i = 0; i < shown; ++i) {
      const int r = first + i;
      const float y = y0 + rowH * static_cast<float>(i);
      if (r == x.cursor) Box(rc, x0 - 6, y, 506, y + rowH - 2, sel);
      Text(rc, x.rows[static_cast<size_t>(r)].label, x0, y + 2, x.rows[static_cast<size_t>(r)].disabled ? grey : white, 16.0f);
    }
  }
  if (x.prompt) {
    Box(rc, 170, 190, 470, 262, panel);
    Text(rc, "Drop out?", 320, 198, colour, 20.0f, EFontAlignX::Center);
    Text(rc, "A: drop out    B: keep playing", 320, 230, white, 15.0f, EFontAlignX::Center);
  }
  if (!x.note.empty() && _framecount < x.noteUntil) Text(rc, x.note, 320, 380, white, 16.0f, EFontAlignX::Center);
}
}  // namespace

void Mp4DrawHud(ERC* rc) {
  if (!rc || !simmy::split_windows_frame()) return;
  const bool notes = !g_note.empty() && _framecount < g_noteUntil;
  if (!Mp4AnyJoined() && !notes) return;
  ENgcRenderStateCache* cache = _ngcRenderer.GetRenderStateCache();
  if (notes) {
    // A join that could not happen: in player 1's window.
    simmy::display_set_hud_window(0, true, 0);
    if (cache) cache->m_dirty |= 0x80000600;
    Text(rc, g_note, 320, 420, EVec4{1, 1, 0.6f, 1}, 18.0f, EFontAlignX::Center);
  }
  const int windows = simmy::split_window_count();
  for (int p = kGcPlayers; p < kMaxPlayers && p < windows; ++p) {
    if (!g_x[p].joined) continue;
    simmy::display_set_hud_window(p, true, 0);  // laid out over the whole stage, fitted into window p
    if (cache) cache->m_dirty |= 0x80000600;
    DrawPlayer(rc, p);
  }
  simmy::display_set_hud_window(-1, false, 0);
  if (cache) cache->m_dirty |= 0x80000600;
}

bool Mp4PadKeptFromUi(int pad) {
  for (int p = kGcPlayers; p < kMaxPlayers; ++p)
    if (g_x[p].joined && g_x[p].pad == pad) return true;
  // Extra players may join this game: an unmapped pad's Start is a join, not player 1's pause.
  return simmy::settings_max_players() >= 3 && _gd.m_fieldEC && _gd.m_fieldE8 && _globals.m_house;
}

bool Mp4PadOfExtraPlayer(int pad) {
  for (int p = kGcPlayers; p < kMaxPlayers; ++p)
    if (g_x[p].joined && g_x[p].pad == pad) return true;
  return false;
}
