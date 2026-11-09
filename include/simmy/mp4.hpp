// PC mod: 4 players (src/game/mp4.cpp, docs/multiplayer.md).
//
// The GameCube game has two players. With players=3 or 4 in simmy.ini (SIMMY_PLAYERS), a two-player
// Free Play game on a lot whose household has more sims than players takes up to two more: player 3
// and 4 press Start on their own controller and take the next household sim no player has. Each
// extra player gets a camera (and camera director), the direct control of their sim (left stick,
// A for the action menu, B cancel, X/Y switch sim), a plumb bob in their colour, a small PC HUD
// (their sim, its action and motives, the action menu) and their own window (split=windows). Start
// asks to drop out. Players 1 and 2 are the GameCube's and keep its code paths unchanged; extra
// players are dropped when the game leaves the lot (quit to the menu, lot change) and are never
// written to the save.
#pragma once

#include <vector>

struct ERC;
struct cXObjectImpl;
struct cXPerson;

// Is `player` (2 or 3) an extra player in the game now?
bool Mp4Joined(int player);
// How many player slots live mode runs (the views, the interactors, the cameras): 1 or 2 as on the
// GameCube, 1 + the highest extra player while extra players are in.
int Mp4PlayerSlots();
bool Mp4AnyJoined();
// Each live-mode update (ELiveMode::Update, before the interactors): joining, dropping out, the extra
// players' menus.
void Mp4Update();
// The extra players' PC HUD, after the Apt UI (ESimsApp::UpdateDraw).
void Mp4DrawHud(ERC* rc);
// Leaving the lot (InteractorModule::ShutdownInteractorModule, the quit to the main menu): every extra
// player drops out (nothing of theirs is saved).
void Mp4DropAll(const char* why);
// An extra player's action menu (DirectInteractor::ExecuteActionMenu): the objects their sim can use.
// False: nothing to offer.
bool Mp4ShowActionMenu(int player, const std::vector<cXObjectImpl*>& objects);
bool Mp4MenuOpen(int player);
void Mp4CloseMenu(int player);
// The extra players' plumb bob colours (players 3 and 4: red, yellow).
const float* Mp4PlayerColor(int player);
// The index of a player's per-player state: the player for players 1 and 2 (as the GC's `player & 1`)
// and the extra players.
inline int Mp4Slot(int player) { return player >= 2 && player < 4 ? player : player & 1; }
// The objects an extra player's sim could use now (as DirectInteractor::UpdateObjectHighlights picks
// them; the extra players light no highlight flags, which the GC has for two players only).
void Mp4CollectHighlightable(int player, std::vector<cXObjectImpl*>& out);
// The pad does not drive the GC UI as player 1 (AptViewer::NewReadController's unmapped pads): it is
// an extra player's, or extra players can join now and its Start is theirs.
bool Mp4PadKeptFromUi(int pad);
// The pad is an extra player's (GameData::SystemPreUpdate does not give it to player 1 or 2).
bool Mp4PadOfExtraPlayer(int pad);
