# fix7: co-op food creation ("Get Ingredients") belongs to the player who opened it

Tree /home/claude/gfx/port (base73 copy). Build /home/claude/gfx_build (Ninja, clang, RelWithDebInfo,
SIMMY_AURORA_DIR=/home/claude/gfx/aurora with the two-window patch; the cache already matched simmy_build's
options). Full diff vs base73 (gen/ and __pycache__ left out): $S/fix7/session_full.diff (15 files).
Binaries: $S/fix7/bin/simmy_before (base73 + the SIMMY_INTERACT_PICK_PLAYER dev switch only) and
$S/fix7/bin/simmy_final (= this tree). Scripts: $S/fix7/os.sh (coop2w/os.sh with SIMMY_TWO_PLAYER=${TWO:-1}),
$S/fix7/hl.sh, $S/fix7/suite/ (m16 suite pointed at a binary with BIN=, own XDG dirs).
Outputs: /mnt/user-data/outputs/fix7_p1.png, fix7_p2.png, fix7_both.png (960x540 windows: player 1 in
food creation, player 2's action menu open at the same moment), fix7_modes.png (player 2 cooking in
window 2 while player 1 walks with the dish they made; split=gc before/after; split=wide after).

## The user's report
"When one player opens the fridge to pick ingredients, it shows the HUD on the other person's screen as
well, and the other person cannot do anything while it is open."

The fridge's "Get Ingredients" (refrigerator_cheap, check 2009 / action 2003) runs generic sim call 0x27
(GenericSimCallXamMode, 80102A24) -> GameData::GotoXamMode(player, 0xC, food object, fridge level) ->
flow "MixIngredients" (0x19) -> INGTarget (screen 0x19 "ING", s2c_ing_foodcreation). Its recipe book is
RCPTarget (0x1A "RCP"); its help pages and "leave without ingredients?" prompt are dialog-box (G2D)
dialogs (HelpDialog).

Repro b1 (simmy_before, windows 640x360, lot 13, SIMMY_INTERACT_PICK=Ingredients for player 1):
- "ING" (a root instance of the movie, 7..640 x 25..478: the whole stage) was drawn twice by the
  two-window HUD routing: its panels across the middle went to both windows, its right-half parts
  (plate, button legend) to player 2's window only. Player 2's window showed most of player 1's screen.
- Every screen was suspended (both HUDs), player 2's pad did nothing (stick, A, d-pad), and the
  simulator log showed `speed x0.0, stalled 1` for the whole time: the lot was frozen.

## What the GameCube did (decomp, targets_s2c.c / inlevel.c / the port's faithful code)
It was modal for both players, on purpose:
- INGTarget::INGTarget (801B9468): `pushAptButtonFilterALL(0, "INGTarget")` and, when DAT_80475C70 (two
  players), `pushAptButtonFilterALL(1, ...)`: every command of both pads goes to the screen; the
  game's interactors (camera filters) see nothing. RCPTarget (801DCF40) does the same.
- UIScreenDataTable: ING and RCP have suspend mode 1: LoadUIScreen suspends every active screen
  (both players' HUDs, the H2D buttons). The screen is laid out over the whole 640x480 stage; on one TV
  it covered both halves.
- InLevelBaseState::Update case 0x19 (80089F2C) drops both players' interactor input.
- The wait is modal: GenericSimCallXamMode returns 4 until INGTarget::end_dialog answers. A primitive's 4
  makes TreeSimImpl::Simulate return 0 and cSimulatorImpl::TickAllObjects stop at that object every tick
  (IsStopped, speed 0): the whole lot stands still (no pause flag, a stall). The dialog box's help/exit
  dialogs additionally pause the simulator (G2DTarget::SpawnDialog: H2DTarget::IncPause).
- Only one at a time: one INGTarget (DAT_80475BF0), one flow state, one "ING" movie instance, and the
  answer goes through the global cXObjectImpl::overrideDialogReturnCode.
So this was case (b). The port reproduced it faithfully, and the two-window routing then spread the
full-stage screen over both windows (the "HUD on the other screen").

## Fix (PC co-op improvement, all split modes; one player unchanged)
With two players (DAT_80475C70) food creation becomes a "private screen" of the player who opened it.
1P keeps the GC path byte for byte (filters, suspension, stall, overrideDialogReturnCode).
- Drawing (pc_hud.cpp, pc_splitscreen.cpp, platform.hpp "Co-op private screens"):
  coop_set_private_screen("ING"/"RCP"/"0G2D", player). A root instance of that name goes to its
  player's window only (split=windows: whole stage, fitted at the window's height, anchors as for
  dialogs; hud=edges then puts the ingredient panel and the plate at the window's edges as in 1P
  edges). In one-window split frames (gc, 4x3, wide) display_map_viewport maps just that instance into
  its player's half (MapWindows with the half's aspect: gc 2:3 -> half scale, wide 8:9 -> 2/3, 4x3 ->
  full size); everything else is drawn as before (identity). In both cases the rest of the owner's HUD
  (what lies in their half of the stage) is not drawn meanwhile.
- Screens (uiscreens.cpp): loading ING/RCP (and the dialog box when it shows their dialog) suspends only
  the owner's per-player HUD screens (0x32-0x3F of their parity) and other non-HUD screens; the other
  player's HUD, H2D (0x31), the fire meter, modeless dialogs and player 1's mood widget 0x34 (it holds the
  money shown in player 2's half) stay up.
- Pads: INGTarget/RCPTarget push their button filter on the owner's pad only. The movie gives its input
  to the screen on top, so the other player's H2D buttons would not arrive: AptViewer::
  OnePlayerReadController sends that player's d-pad / B / A / Start straight to H2DTarget::SetVariable
  (as the movie would, "<player>" value) and nothing to the movie (unless a shared dialog box is up).
  H2DTarget::SetVariable ignores the owner's H2D buttons, and holds off the other player's Start
  (pause) while the screen is up (logged "[coop] player N: pause held off ..."): the pause menu would
  pause and suspend food creation and its "ingame" exit deletes INGTarget.
- Simulation (ingamecas.cpp): the opener's XAM call waits with 2 (yield: the sim stands at the fridge,
  re-runs the node next tick) instead of 4 (stall the world). Time, motives, NPC autonomy and the other
  player continue; food creation costs game time in co-op (it did not on the GC). The answer goes
  through PcXamDialogDone to that sim only (s_xamOwner/s_xamResult), not overrideDialogReturnCode,
  which an object dialog of the other player's uses too. Another sim asking meanwhile waits its turn
  (2); an answer its sim never takes is dropped after 60 other calls; INGTarget destroyed without an
  answer gives "no dish" (PcXamDialogClosed).
- Dialog box (g2dtarget.cpp, ingtarget.cpp): a dialog for the owner while their private screen is up
  (help pages, exit prompt; m_players == owner+1) is private too: no IncPause/ReleasePause, filter on
  the owner's pad only, drawn in their window/half ("0G2D" registered). The exit prompt now sets
  m_players to the owner in co-op (GC left whatever the last help page had, else "either player").
- Lot-wide actions wait while a private screen is up (on the GC the lot was frozen then): generic sim
  call 0x22 (wardrobe -> in-game CAS, leaves the lot) and 0x16 (TransitionToNewHouse: leave lot) return
  2 until it closes.
- Dev: SIMMY_INTERACT_PICK_PLAYER=1|2 limits SIMMY_INTERACT_PICK to that player (the other one's A opens
  the real action menu). README: a paragraph under the graphics settings.

## Other full-screen / per-player UIs checked
| UI | mechanism | shares the problem? | done |
|---|---|---|---|
| Food creation ING, recipe book RCP, their help/exit dialogs | object call 0x27 -> flow 0x19, no pause, modal stall | yes | made private (above) |
| Object dialogs (phone, computer, mailbox/bills, career, job offers, "Pick Up" choices...) | EdithDialogPrimitive -> G2D dialog box; TryDialog returns 4 (stall) and G2D pauses the simulator; both pads filtered | shown in both windows and blocks both, but they freeze the shared simulation by design (an answer the tree waits on) | unchanged (GC). Making them per-player would mean running the lot while a sim waits on a modal answer; left as an option |
| Pause menu and its submenus (wants & fears WXF, progress PRG, career/skills CSP, relationships R2L, buy/build catalogue PCT, options O2T, save M2M) | flow "Pause": H2DTarget::IncPause, both pads | both windows, both blocked: the game is paused for both, as on GC | unchanged |
| Reward moment RMD | IncPause, both pads | same as above | unchanged |
| Wardrobe (in-game CAS), leave lot, story end movie | leave the lot | full-screen for both by nature | unchanged; they now wait while a private screen is up |
| Tombstone placement (death) | pause + PCT | paused | unchanged |
| Wants/motives/skills side panels, action menu, social menu, interest meter, action queue | per-player HUD widgets | no (already per player/window) | - |
No other GenericSimCall or flow opens a non-pausing full-stage screen (GotoXamMode modes < 0xC only set UIDB values).

## Verification (offscreen unless noted; logs and shots in $S/fix7/out, $S/fix7/logs, $S/fix7/suite)
| run | binary / mode | case | result |
|---|---|---|---|
| b1 | before, windows | P1 Get Ingredients, P2 stick/A | ING spread over both windows, all HUD gone, P2 ignored, sim stalled (repro) |
| t1 | before, HUD trace | instance tree | "ING" root instance 7..640 x 25..478 |
| gc_base | before, split=gc 1280x960 | same | GC behaviour: full screen over both halves, lot frozen |
| f3 | v3, windows | P1 in ING; P2 walks, A (menu "Fridge / Winston"), d-pad motives | ING in window 1 only; P2's menu and motives in window 2; speed x1.0; P1 Done -> "made a dish", food_object takes recipe 1 |
| fin2 | final, windows 960x540 | P1 in ING; P2 A (menu), B, d-pad motives, Start | 1CXM_show at once (outputs fix7_*); 1MOT show/hide; "[coop] player 2: pause held off"; P1 a,x,down,a -> made a dish; "ING is no longer private" |
| g1 | v4, windows | P2 opens ING; P1 moves, motives, Start; P2 recipe book (Y), back, B -> exit prompt, yes | ING private to player 2 (window 2), P1's HUD intact; "player 1: pause held off"; RCP private to player 2 (window 2); P2 left without a dish. (The exit prompt was still shared there; fixed after, see dlg) |
| f4 | v6, windows 960x540, hud=edges | both players pick Get Ingredients | P1's ING; P2's queued Get Ingredients cannot reach the fridge P1 stands at (route failure 13, as for any object in use) and ends; P1 makes a dish (funds 1000 -> 994, P1 carries the plate); P2 then opens their own ING in window 2 and makes a dish while P1 walks around |
| dlg | final, windows | P1 help pages (Z) while P2 plays | "0G2D is private to player 1", filter on p1 only, speed x1.0 throughout; P2's A opens their menu under it; page 2, B closes |
| dlg2 | final, windows | P1 exit prompt (B) while P2 plays, then yes | prompt in window 1 only (out/dlg2_1150_both.png), no pause (speed x1.0), P2's menu opens meanwhile; "left without a dish", both privates released |
| gc_new | v6, split=gc | P1 ING, P2 menu/motives | ING at half scale in P1's half, P2's half normal and moving (fix7_modes.png) |
| wide | v7, split=wide 1280x720 | P1 ING | ING fitted into P1's 8:9 half, P2's half normal |
| r2base / r2new | before / final, 1P | ING_TEST + recipe book | identical frames; 1P sim still stalls under ING (GC) |
| suite | final, headless | toilet / sofa / fridge / shower / bed / cancel | 56.7 / 28.3 / 31.7 / 45.5 / 32.7 (same as before binary run alone: 56.7 / 28.3 / - / 45.6); `grep -c "cancels: 1" s_cancel.log` = 1; 0 TREEASSERT, script errors or "keeps failing". (A first run while two offscreen runs shared the 2 CPUs gave 59.0/29.3/47.8 with both binaries' timing: wall-clock pad waits) |

## Left / notes
- Two players cannot be in food creation at the same moment: there is one "ING" movie instance, one
  INGTarget and one flow state, and one fridge slot (the second sim's walk fails while the first stands
  there). The second player can cook right after; a second sim reaching the XAM call meanwhile waits.
- While a private screen is up the other player cannot pause, use a wardrobe or leave the lot (they wait).
- Food creation now costs game time in co-op (the lot runs). An NPC that wants the fridge retries it
  every ~25 frames while it is occupied (normal autonomy for an object in use; it was frozen before).
- Object dialogs (G2D) are still shared (both windows, both paused) as on the GC.
- In split=gc the private screen is half size (320x240 of the 640x480 frame); 4x3 shows it full size.
- AptViewer::PopAptButtonFilter pops the player's top filter whatever the id (GC). With private filters on
  one pad only the order stays LIFO per pad except if food creation closes while a shared dialog is up.
- MSVC: no VLAs/extensions, no operator or using changes; C++17 if-with-initializer, std::vector,
  std::snprintf/strncmp (headers already included).
