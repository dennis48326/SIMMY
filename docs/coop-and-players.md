# Co-op and players

## Overview

The GameCube game supports **two players** on one TV: split-screen live mode, a HUD per player, one
shared simulation, one lot, one family. The port keeps that and adds PC display layouts (4:3 halves,
wide, two windows) and "private screens" so one player's food creation no longer freezes the other. This
page is also the inventory for any future 3-4 player work: the number two is built into the game's
data, its Flash movies and much of its code.

## Turning it on

- Front end: the players screen offers two players only when two controllers are connected
  (`MMU_Is_Two_Player_Enabled`, uiscreens.cpp); the choice calls `GameData::SetTwoPlayerMode`
  (UIDB `TwoPlayerGame`, **`_gd.m_fieldEC` = DAT_80475C70**, the two-player flag everything tests,
  often through a local `TwoPlayers()` helper).
- Dev: `SIMMY_TWO_PLAYER=1` sets it in `PreGameState` (pregame.cpp); the launcher's "quick co-op" adds it
  with `SIMMY_CAS_ACCEPT=300` and `SIMMY_SKIP_INTRO=1`.
- Pre-game: player 1's CAS, then player 2's roommate (`PreGameState::InsertPlayer2Roommate` 800948D4:
  family 2's first member moves in; neighbourhood var 0x13 = player 2's neighbour) and player 2's CAS. Player
  `p`'s own sim is the player family's member `p` (`Player::GetPrimaryNeighbor`).

## Player state (all per player, index 0/1)

| State | Where |
|---|---|
| `Player m_players[2]` (camera, pie menu, primary neighbour) | `GameData` +0xF4 ([`gamedata.hpp`](../include/simmy/classes/gamedata.hpp)) |
| Selected sim `m_selectedPerson[2]`, last-used control mode `m_usedSimInteractor[2]`, camera directors `m_cameraDirectors[2]` | `EGlobal` ([`eglobal.hpp`](../include/simmy/classes/eglobal.hpp)) |
| Pad mapping | `EControllerManager` (`MapPlayerToController`) |
| Camera tuning `ESimsCam::s_params[2]` | camera.hpp |
| Plumb bobs `s_pPlumbBob[2]` (player 1 green, player 2 blue), interactor visualizer draw records `m_otd[2]` | siminteractor.hpp |
| Apt button filters `m_filters[2][10]`, key repeat state | aptviewer.hpp / aptviewer.cpp |
| HUD: action menus `m_menus[2]`; H2D widgets `m_mod/m_csm/m_mot/m_skl/m_waf/m_act/m_int[2]`; per-target `m_filters[2]` (PAZ, PCT, CSP, PRG, R2L, WXF) | hudtarget.hpp etc. |
| Per-player highlight caches (`m_hasInteractions[2]`, `m_hlTime[2]`) and fades (`EInstance::m_alpha[2]`, `ERoom::m_partAlpha[2]`) | isimsobjectmodel.hpp, elevel.hpp, eroom.hpp |
| Memory card per player (`s_playerCard[2]`) | thesims.hpp |
| UI names: `"0..."` / `"1..."` prefixes; screens 0x32..0x3F in pairs (player 1 even, player 2 odd) | the Flash movies and targets ([ui.md](ui.md)) |

## Live mode with two players

- **Views**: `ELiveMode::DrawMain` draws two halves (`InitPlayerXWindow`, each at half the frame
  aspect), each with its player's camera and director selected, then the divider. If a player's camera is
  full screen, or a player is in build/buy (`GameData::SetBuildBuyPlayer`), only that view is drawn, full
  frame (the BBH HUD and fire code meter are full-width layouts).
- **Display modes** (`split=gc|4x3|wide|windows`, live-switchable): [rendering.md](rendering.md).
- **Update**: `UpdateDirectInteractors(2, ...)` and both cameras; `ELiveMode::Update` scales `_gamedt` by
  0.5 in two-player games (as ported from GC).
- **Sim selection**: Y skips the other player's sim (`EGlobal::SimInUseByOtherPlayer`).
- **Social mode**: one player can socialise with the other's sim; `CheckSecondPlayerForFailedSocialModeEntry`
  (8012B978, two players only) and `CheckNewSimInSocialMode` (80213C60) handle the second player.
- **Audio**: one listener per player camera, summed into one stereo mix ([audio.md](audio.md)).
- **Input**: keyboard/mouse is player 1's (port 1); the mouse only counts inside player 1's window/half
  ([input-and-control.md](input-and-control.md)).

## Shared vs per-player UI

| UI | Mechanism | Co-op behaviour |
|---|---|---|
| HUD widgets, action menu, social menu, interest meter, action queue, side panels | per-player targets and screens | per player (left half / right half; own window with `split=windows`) |
| Pause menu and all its submenus (WXF, PRG, CSP, R2L, catalogue, O2T, M2M) | flow "Pause", `IncPause`, both pads filtered | shared: both windows, game paused for both (GC) |
| Object dialogs (phone, bills, job offers, ...) | `TryDialog` -> G2D dialog box; the tree waits on the answer; `IncPause` | shared, both paused (GC; making them per-player would mean running the lot while a tree waits on a modal answer) |
| Reward moment (RMD), tombstone | pause | shared |
| Wardrobe (in-game CAS), leave lot, story end movie | leave the lot | full screen for both by nature |
| **Food creation (ING), recipe book (RCP), their help/exit dialogs** | object call 0x27 -> flow 0x19 | **PC private screen** of the opener (below) |

### Private screens (PC, fix for "the other player cannot play while I cook")

On GC, food creation pushed `pushAptButtonFilterALL` on both pads, suspended every screen (suspend mode
1), and the opener's XAM call returned 4 so `TickAllObjects` stalled every tick: the lot froze. With two
players the port makes it the opener's own screen; one player keeps the GC path byte for byte.

| Part | Code |
|---|---|
| Drawing | `coop_set_private_screen("ING"/"RCP"/"0G2D", player)` ([`pc_splitscreen.cpp`](../src/platform/pc_splitscreen.cpp), [`pc_hud.cpp`](../src/platform/pc_hud.cpp)): that root instance goes only to its player's window (`windows`) or is fitted into their half (`gc`/`4x3`/`wide`); the rest of the owner's HUD is hidden meanwhile |
| Screens | `UIScreenManager::LoadUIScreen` suspends only the owner's HUD screens (`PrivateSuspendList`, uiscreens.cpp) |
| Pads | `INGTarget`/`RCPTarget` filter the owner's pad only; `AptViewer::OnePlayerReadController` sends the other player's d-pad/A/B/Start straight to `H2DTarget::SetVariable`; the other player's Start (pause) is held off while the screen is up |
| Simulation | `GenericSimCallXamMode` ([`ingamecas.cpp`](../src/game/ingamecas.cpp)) returns 2 (yield: the sim waits at the fridge) instead of 4 (stall); the answer goes to that sim only (`PcXamDialogDone`, not the global `overrideDialogReturnCode`) |
| Dialogs | a help/exit dialog for the owner while their private screen is up is private too (no `IncPause`, owner's pad, drawn in their half) (g2dtarget.cpp) |
| Lot-wide actions | wardrobe (call 0x22) and leave-lot (0x16) wait (return 2) while a private screen is up |

Limits: one ING movie instance, one `INGTarget`, one flow state: two players cannot be in food creation
at once (the second waits). Food creation costs game time in co-op.

## Hard-coded to two (inventory for 3-4 players)

- **Data and movies**: the Flash movies have exactly two players' variable names and layouts
  (`0`/`1` prefixes, screen id pairs 0x32-0x3F, the two-player HUD laid out in stage halves). The memory
  card save has two player slots and the family flags `kP1DoneCAS`/`kP2DoneCAS`; neighbourhood var 0x13 is
  "player 2's neighbour". These need new art/data, not just code.
- **Flag**: `_gd.m_fieldEC` is a boolean; dozens of call sites test it (63 occurrences in 37 files) or use
  `TwoPlayers()`.
- **Arrays of 2** (table above), `p < 2` / `i < 2` loops (~60 in ~30 files), and "the other player"
  computed as `player & 1`, `1 - player` or `player ^ 1` (~100 sites in ~25 files: action menu, social
  mode, build tools, HUD, private screens, plumb bob...).
- **Rendering**: `InitPlayerXWindow` splits left/right halves; `split_screen_*` and `MapWindows` assume two
  halves; window 2 is the only extra window (`pc_window2.cpp`); the EFB override is 2 x width.
- **Input**: `TryAutoMapPlayer` loops two players; `KbmPlayer` is the port-1 player.
- **Audio** listeners: two cameras.
- **Range checks**: `GameData::SetBuildBuyPlayer` accepts only -1..1.

## Gotchas

- Always derive "whose" from the target's `m_player` / the screen's player, never assume player 0.
- A full-stage screen one player opens must become a private screen in co-op or it blocks both players.
- With `split=windows`, anything drawn across the middle of the stage is drawn into **both** windows;
  player-specific full-stage dialogs must be registered as private or they show in the other window.
- Test co-op with `SIMMY_TWO_PLAYER=1` and `p2:` names in pad scripts; `SIMMY_INTERACT_PICK_PLAYER` limits
  scripted picks to one player; `SIMMY_COOP_TRACE=1` prints both players' sims/cameras/sticks.
