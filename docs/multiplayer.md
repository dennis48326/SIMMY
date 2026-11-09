# Multiplayer: 3 and 4 players (PC mod)

A **PC mod**, not GameCube behaviour: Free Play with up to four players on one PC, each in their own
window. It is off by default. With `players=1` or `2` (default) the game is the GameCube's (one or two
players, [coop-and-players.md](coop-and-players.md)); nothing below runs and the determinism hash is
unchanged. Network play is not part of it.

Code: [`src/game/mp4.cpp`](../src/game/mp4.cpp) (joining, dropping out, the extra players' HUD and
action menu), [`include/simmy/mp4.hpp`](../include/simmy/mp4.hpp), the windows in
[`pc_window2.cpp`](../src/platform/pc_window2.cpp) and [`pc_splitscreen.cpp`](../src/platform/pc_splitscreen.cpp).
Every change in GC-ported files is marked `PC mod: 4 players`.

## Turning it on

1. `players = 4` (or 3) in `simmy.ini` (`[game]` section; also `--players 4`, `SIMMY_PLAYERS=4`,
   `SIMMY_GFX="players=4"`), and `split = windows` (the co-op display). For 480p windows:
   `window = 640x480`.
2. Start a **two-player Free Play** game the GameCube way (main menu, Free Play, two players, a lot).
3. Players 3 and 4 press **Start** on their own controller (SDL gamepads 3 and 4; any controller no
   player has). Each takes the first sim of the household no player has, and a window
   "SIMMY - Player N" opens (window 1's size, under windows 1 and 2).

A player cannot join (a note in player 1's window and the log says why) in Story Mode (two players at
most there), in a one-player game, with another `split` display, while build/buy, a pause, a dialog
or the lot's intro camera is up, or when every household sim already has a player. A lot needs a
household of at least three (four) sims, e.g. lot 13 (the Freebers, four sims).

**Dropping out**: Start, then A at "Drop out?" (B or Start keeps playing), or close the player's window.
The sim goes back to autonomy and the window closes. The others play on. A player can join again later.

## What an extra player has

| | Players 1 and 2 (GameCube) | Players 3 and 4 (PC mod) |
|---|---|---|
| Sim | from the family (pre-game) | the first free household sim at join; X / Y switch to another free one |
| Control | direct (stick) or classic cursor (Z) | direct control only |
| A | the HUD's action menu (Flash) | a PC action menu in their window: the interactions of the objects in front of the sim (the GC's own lists: `CollectInteractionsForObject`), d-pad up/down, A does it, B backs out |
| B | cancel | cancel |
| Camera | `ESimsCam` + `CameraDirector` per player | the same, own instances (tuning copied from player 1's) |
| Plumb bob | green / blue | red / yellow |
| HUD | Flash HUD (mood wheel, motives, queue, clock, money) | a PC HUD drawn with the game font: name, current action, the eight motives |
| Start | pause menu (pauses everyone) | the drop-out prompt |
| Build/buy, social-mode menu, food creation, classic cursor | yes | no |
| Window | window 1 / window 2 | windows 3 / 4 |

The pause menu, object dialogs, reward moments and hints are shared: they show in every window and
pause everyone, as the GameCube's two-player game does for both players.

## How it works

- **Player slots.** `kMaxPlayers` (4, `include/simmy/port.hpp`) sizes the per-player arrays the extra
  players use: `EGlobal::m_selectedPerson`, `m_cameraDirectors`, `m_usedSimInteractor`,
  `GameData::m_players`, `ESimsCam::s_params`, `PlumbBob::s_pPlumbBob`, the interactor visualizer's draw
  records, the direct interactors. Slots 0 and 1 keep the GameCube layout and code; the GC's `player & 1`
  becomes `Mp4Slot(player)` where an extra player can reach it (the same for players 1 and 2).
- **Loops.** `ELiveMode::Update` / `DrawMain` and `UpdateDirectInteractors` run `Mp4PlayerSlots()`
  slots (1 or 2 as before; 1 + the highest extra player). Loops that ask "is this sim a player's"
  (autonomy, routing, idle animation, object kills, the family leaving the lot, neighbour reputation,
  menu building) cover all four slots; they find nothing in the extra slots in a GC game.
- **What extra players do not reach.** Classic control, the build/buy manipulators and tools, mouse
  classic control, social-mode menus and the Flash HUD return "none" for players 3 and 4
  (`PlayerInteractorType`, `PlaceManipulatorInfo`, `UpdatePlaceManipulator`, build tools,
  `EGlobal::SetSelectedPerson` skips the simulator globals 3 / 0x20 and the HUD for them).
- **Selection.** `EGlobal::SimInUseByOtherPlayer` checks every other player, so players 1 and 2 skip
  the extra players' sims when they switch, and the other way round. Joining uses
  `PcFirstFreeFamilyMember` (familyselect.cpp: the GC's `SelectableMember` checks over the player family).
- **Pads.** The controller manager already has four player slots: an extra player is mapped to their
  pad (`MapPlayerToController`). `AptViewer::NewReadController` leaves out the extra players' pads (and,
  while extra players can join, pads no player has: their Start is a join, not player 1's pause);
  `GameData::SystemPreUpdate` never gives an extra player's pad to player 1 or 2.
- **Highlights.** The GC has highlight flags for two players (1 and 8); extra players' sims light no
  objects. `Mp4CollectHighlightable` picks the same objects `UpdateObjectHighlights` would.
- **Drawing.** An extra player's view draws in the second view's context (`EInstance::SetContext(1)`,
  the per-view wall and fade state: it is worked out again from the camera before each view).
- **Windows.** The split frame is cut into one strip per window (`split_set_window_count`): the EFB is
  n windows wide, `ELiveMode::InitPlayerXWindow` gives player k strip k, window k presents strip k
  (`set_present_crop`). The GC players' Flash HUD stays laid out in the stage's halves and goes to
  windows 1 and 2 as before (`MapWindows`); whole-stage dialogs are drawn once per window
  (`simmy_apt_hud_enter` passes). With two players the strips are the old halves.
- **aurora patch 0002** (`cmake/aurora_patches/0002-larger-frame-buffers.patch`): the per-frame vertex and
  index staging grow from 5 / 2 MiB to 12 / 4 MiB (four views overran 5 MiB; aurora aborts then).
- **Saving and quitting.** Nothing of the extra players is saved (the GameCube save has two players).
  Leaving the lot (`InteractorModule::ShutdownInteractorModule`: quit to the main menu, a lot change)
  drops them first.

## Testing

| Variable | Does |
|---|---|
| `SIMMY_PLAYERS=4` | `players=4` |
| `SIMMY_FAKE_PADS=4` | scripted pads 1..4 plugged in from the start |
| `p3:` / `p4:` in `SIMMY_PAD_SEQ` / `SIMMY_PAD_SCRIPT` | press on pad 3 / 4 (`p3:start` joins) |
| `SIMMY_MP4_START="3:x,y,4:x,y"` | the joining player's sim is put on that tile |
| `SIMMY_MP4_OBJECTS="3:27,4:23"` | that object (id) heads the player's action menu |
| `SIMMY_MP4_PICK="3:Use,4:Bath\|Shower"` | the menu cursor starts on the first row with that text |
| `SIMMY_MP4_TRACE=1` | Start presses per pad, the lot's objects at join |

Offscreen runs make virtual windows 3 and 4: `--screenshot x.bmp` also writes `x_p3` / `x_p4`. Example
(lot 13, players 1-4 at the fridge, sofa, toilet and bath):

```
SIMMY_TWO_PLAYER=1 SIMMY_PLAYERS=4 SIMMY_FAKE_PADS=4 SIMMY_GFX="window=640x480;split=windows"
SIMMY_INTERACT_PICK=Snack,Sit SIMMY_INTERACT_NEAR="#21;#83" SIMMY_MP4_START=3:10,17,4:11,19
SIMMY_MP4_OBJECTS=3:27,4:23 SIMMY_MP4_PICK=3:Use,4:Take SIMMY_PAD_SEQ_UI=1
SIMMY_PAD_SEQ="start,right,a,right,a,up,a,a,a,a,house,a,a,p2:cleft/10,wait/60,p3:start,wait/60,p4:start,wait/100,a,p2:a,p3:a,p4:a,wait/60,p3:a,p4:a,wait/3000"
```

(`p2:cleft` first: with `SIMMY_TWO_PLAYER` player 2's pad is only settled once player 2 uses one, as the
GameCube's "first other pad in use" rule in `GameData::SystemPreUpdate`.)

## Performance

Four views draw the lot four times (one EFB, four strips; the GX command stream per frame grows from
about 2.1 MB with two players to 4.3 MB with four). Measured offscreen (Linux, Mesa software
rendering, forced readback, four 640x480 virtual windows, lot 13, the same scripted run): the game's
main-thread time per frame (`SIMMY_FRAME_STATS`, game + aurora end) is about 16-20 ms with four players
against 10-13 ms with two, and the whole run took 337 s against 217 s. Real-GPU frame rates were not
measured (no GPU or display here): most of the extra cost is per-view CPU work (the order table, GX
command building), roughly linear in the number of views; whether a given PC holds 60 fps with four
windows is untested.

## Limitations

- Free Play only, two-player games only (players 1 and 2 are the GameCube's), `split=windows` only.
- Extra players have direct control only: no classic cursor, build/buy, social mode ("Socialize" is not
  in their menu: the social menu has two players' screens only, and their sim would wait in it), food
  creation screen, pause menu of their own. The other social interactions (Talk, Tell Joke, ...) work.
- Their HUD is the PC one (no mood wheel, wants, skills, money; no action queue cancelling).
- No object highlights for extra players; the "rings" under other sims in their view are the second
  view's.
- Audio: two listeners (players 1 and 2's cameras), as before.
- Windows 3 and 4: not remembered in simmy.ini, not vsynced, no fullscreen toggle; keyboard and mouse
  stay player 1's.
