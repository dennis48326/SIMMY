# UI: Apt movies, targets, screens, the HUD

## Overview

The whole UI (front end, CAS, HUD, pause menus, dialogs, catalogues) is **Flash**, played by EA's Apt
player (`src/libaptz`, reimplemented for PC) from `flashes.arc`. The movies do layout, animation and
navigation; the game talks to them only through **variables**: a movie calls `getVariable` /
`setVariable` / localised-string lookups on names like `"0H2D_PAUSE"` or `"uidb:CAS_mode"`, and
game-side **targets** (`UIObjectBase` subclasses) installed those names in global command tables. The
game loads screens (`UIScreenManager::LoadUIScreen`), pushes values (`UIDB`, `UIReflow` tags) and sends
flows. Everything per player carries the player digit (`0...` / `1...`) and per-player screen ids.

## Apt runtime (`src/libaptz`)

| Piece | File | Notes |
|---|---|---|
| Loading | [`aptfile_pc.cpp`](../src/libaptz/aptfile_pc.cpp), [`aptfile.hpp`](../include/simmy/classes/aptfile.hpp) | `AptFileImage::Build` parses `.apt`/`.const` into native structs (64-bit safe); strings and bytecode stay in the retained big-endian images and are decoded on access ([PORTING.md](../PORTING.md) "Apt movies on PC") |
| Values, objects, GC | [`aptvalue.cpp`](../src/libaptz/aptvalue.cpp), [`aptruntime.hpp`](../include/simmy/classes/aptruntime.hpp) | `AptVar`, `AptObject` property maps, mark-and-sweep `AptGC` |
| Interpreter | [`aptactioninterpreter.cpp`](../src/libaptz/aptactioninterpreter.cpp) | AVM1 + Apt's ops (0x59-0x76, 0xA1-0xB8). **SWF 6 semantics**: `AptGetSwfVersion` is always 6 on GC, so `"0"` is false, null becomes undefined in equality, numbers compare within 0.001, `isNaN`/`Boolean()` quirks. Inside `with`, a plain name names the with-object unconditionally (GC `getContext` 8026D958) |
| Player | [`aptplayer.cpp`](../src/libaptz/aptplayer.cpp) | Display lists, timelines, clip events, action queue. Queued frame actions carry a frame marker; a stale one is dropped (GC `runActions` 80283FA0), so a script's `gotoAndPlay` is not undone by a child's queued `stop()` |
| Built-ins | [`aptbuiltins.cpp`](../src/libaptz/aptbuiltins.cpp) | Script-created clips live 0x4000 above timeline depths (`kAptScriptDepthBase`), as GC's attachMovie/createEmptyMovieClip/swapDepths |
| Movie lifetime | [`apt.cpp`](../src/libaptz/apt.cpp) | 48 movie levels and 350 texture slots in the aux layer. After each GC collect `AptReleaseUnusedFiles` frees files no live instance uses; an already loaded movie is shared while a clip still shows it. A failed load gives its level back; "no free movie level" is logged |
| Original methods | [`apt_original_methods.cpp`](../src/libaptz/apt_original_methods.cpp) | Addresses of GC Apt methods whose behaviour the PC runtime covers (not ported one by one) |

Game side of Apt: [`aptauxpceorgl.cpp`](../src/game/aptauxpceorgl.cpp) (the aux layer: load, geometry,
textures, drawing through the ERC, text with the game font, custom controls `CCR2D` (a UI2D shader quad)
and `CCR3D` (a UI3D model), masks through the stencil) and [`aptviewer.cpp`](../src/game/aptviewer.cpp)
(`AptViewer`: `UpdateAll` advances Apt by the frame's wall-clock milliseconds, max 66; draws over an
orthographic stage; turns pads into Apt input events; the per-player button filters).

## Targets and command tables ([`uitargets.cpp`](../src/game/uitargets.cpp), [`uitarget.hpp`](../include/simmy/classes/uitarget.hpp))

A target installs names in `GetVariableCommandTable`, `SetVariableCommandTable`,
`GetLocalizableCommandTable` (std::function records keyed by the installing target), plus per-frame
`UIUpdateTable` / `UIDrawTable` entries. `UIRouter` is the handler Apt calls. Built-in routes:
`flowGoto` (`FlowGotoTarget`: flow names -> numbers), `qd:` (`UIQDTarget`: quickdata strings),
`uidb:` (`UIDB`: named ints/strings), `audio:NAME` (`UIAUDIOTarget`, [audio.md](audio.md)). A target must
uninstall everything in its destructor (`UnInstallAllCommands(this)`), or the tables call freed objects.

`UIReflow` (g2dtarget.cpp, hudpanels.cpp) sends layout tags into text fields (`<rot>`, x/y scale, alpha,
font size overrides, swf file); several were generated stubs that silently broke layouts until ported.

## Screens ([`uiscreens.cpp`](../src/game/uiscreens.cpp))

`UIScreenManager::LoadUIScreen(id)` (8017F884) suspends other screens by the screen's suspend mode
(1 all, 3 the HUD widgets, 4 nearly all), then asks the root movie (`s2c_root`) to load it. The table
`UIScreenDataTable` (80424720) gives id, suspend mode, handles-input, player and the flash name or widget
layout ([`widgetlayout.cpp`](../src/game/widgetlayout.cpp)). The root movie gives each pad event to the
**topmost** screen that handles input for that player, then stops.

| Ids | Screen | Target / file |
|---|---|---|
| 0x02, 0x26 | main menu, MMU | `MMUTarget` (uiscreens.cpp) |
| 0x03-0x18 | CAS screens (CGE genetics ... CAR, K2Y keyboard 0x17) | castarget.cpp, cassubtargets.cpp, k2ytarget.cpp |
| 0x19 / 0x1A | ING food creation / RCP recipe book | [`ingtarget.cpp`](../src/game/ingtarget.cpp), [`rcptarget.cpp`](../src/game/rcptarget.cpp) |
| 0x1E | PCT buy/build catalogue | [`pcttarget.cpp`](../src/game/pcttarget.cpp) |
| 0x22 / 0x23 / 0x27 / 0x28 | R2L relationships / CSP career / WXF wants and fears / PRG progress | r2ltarget, csptarget, wxftarget, prgtarget .cpp |
| 0x25 | O2T game options (save, quit, volumes) | [`o2ttarget.cpp`](../src/game/o2ttarget.cpp) |
| 0x29 / 0x40 | BBH build/buy HUD / FCM fire code meter | bbhtarget.cpp, fcmtarget.cpp |
| 0x2A | credits | [`crdtarget.cpp`](../src/game/crdtarget.cpp) |
| 0x2C | PAZ pause menu | [`paztarget.cpp`](../src/game/paztarget.cpp) |
| 0x2F | G2D dialog box | [`g2dtarget.cpp`](../src/game/g2dtarget.cpp), [`helpdialog.cpp`](../src/game/helpdialog.cpp) |
| 0x30 | M2M memory card screens | [`memcardscreens.cpp`](../src/game/memcardscreens.cpp) |
| 0x31 | H2D HUD buttons | `H2DTarget` ([`hudtarget.cpp`](../src/game/hudtarget.cpp)) |
| 0x32-0x3F (pairs: player 1 even, player 2 odd) | CSIM/CXM sim menu, MOD mood, MOT motives, SKL skills, WAF wants and fears, ACT action queue, INT interest meter | csmtarget, hudtarget, hudpanels, acttarget .cpp |
| 0x42 / 0x43 / 0x41 | modeless dialog (MDI) / reward moment (RMD, UI3D model) / intro thought balloon | mditarget, rmdtarget .cpp |
| 0x44 | FAM free play's neighbourhood | [`famtarget.cpp`](../src/game/famtarget.cpp) |
| 0x2B, 0x24 | OPT (its movie is not in this build), E2E (EyeToy) | not ported |

## Flows

UI flows (`flowGoto Name`) become numbers in `FlowGotoTarget` and are served by the current state. In
game, `InLevelBaseState::Update` (80089F2C, [`inlevel.cpp`](../src/game/inlevel.cpp)) handles: 8 ingame,
9 HUD transition, 0x15 Pause (`PAZTarget` for the player who pressed Start), 0x17 SlotSelect, 1 MainMenu,
0x1A WXF, 0x1D PRG, 0x1E CSP, 0x23 R2L, 0x24/0x25 buy/build, 0x27 game options, 0x29 save, 10 Movie,
5 Wardrobe, 0x19 MixIngredients, 0x2A/0x2B tombstone, 0xB..0x14 PDA (logged: no movie issues them). Others
log `[inlevel] flow request N not ported`.

## The HUD ([`hudtarget.hpp`](../include/simmy/classes/hudtarget.hpp))

`HUDTarget` (`_gd.m_hudTarget`, made by `LiveModeInitState` / the "ingame" flow) serves `HUD_*`
variables, the clock in UIDB, pause counting (`IncPause`/`ReleasePause`), the action menus
(`ActionMenu m_menus[2]`) and makes `H2DTarget` (`_gd.m_h2dTarget`). `H2DTarget` owns per player the widget
targets `m_mod`, `m_csm`, `m_mot`, `m_skl`, `m_waf`, `m_act`, `m_int` (arrays of 2), runs them in a fixed
per-frame order (side panels every sixth update), and handles the d-pad side panels (left motives, up
skills, right wants and fears, down closes), Start (`H2D_PAUSE`: sets `PlayerNumber`,
`Player1FirstName`... from the **selected** sim's neighbour), B cancel (`UpdateCancelButton`),
`SelectedPersonChanged`, `NewWantFearIcon`, `SetMoodMeterValue`. The hourglass (`CXM_hourglass_swf`) shows
while the CXM list is closed and the selected sim waits (`GetIdleState() == 3`) or routes
(`CSMTarget::Update` 801A6E00).

**Pause menu** (`PAZTarget`): tabs (one player) 0 wants/fears, 1 progress, 2 career, 3 relationships,
4 buy, 5 build, 6 EyeToy (skipped), 7 options. The selected tab is remembered across openings
(`sInitialSelectedItem_Root`, never reset: GC). Opening it pauses (`IncPause`); submenus are separate
targets made by the flows above.

**Dialogs**: `G2DTarget::SpawnDialog` pauses the simulator (`IncPause`) for modal dialogs; dialogs time
out after 60 s (`g_g2dDialogTimeout` 804FE284); help pages use `HelpDialog`; trees' dialogs come through
`EdithDialogPrimitive` ([`edithdialogprim.cpp`](../src/game/edithdialogprim.cpp)).

## Widescreen HUD placement ([`pc_hud.cpp`](../src/platform/pc_hud.cpp), PC)

`AptDisplayList::render` calls `simmy_apt_hud_enter`/`leave` around each instance; pc_hud decides an
anchor **top down from the instance's visible stage bounds** and sets a horizontal viewport shift (Apt
coordinates, hit tests and scripts are untouched):

| Anchor | When |
|---|---|
| centre | `hud=centered`; or a compact instance across the middle (dialogs, menus) |
| left / right edge | `hud=edges`: bounds in the stage's left / right half (whole subtree follows) |
| stretch | an untextured shape covering exactly the stage width (fades, bars) |
| bleed | a shape drawn wider than the stage on both sides (the pause dimmer, -320..960) |
| extend | textured art over the whole stage (credits' dimmer, CCR2D quads): drawn unscaled + its outer texel columns smeared over the side bars |
| container | as wide as the stage: its children decide (all agreeing children move together) |

In `split=windows` the same pass first picks a window per instance (left half -> player 1, right half
-> player 2, across the middle -> **both**: drawn twice, `simmy_apt_hud_enter` returns 2). Text fields are
measured by their text, not their box. `SIMMY_HUD_TRACE=<frame>` logs the decisions.

## Rules and gotchas

- **UI must stay per player.** Use the player digit in every installed name, push button filters on the
  owner's pad only, read `GetSelectedPerson(p)` (not the primary neighbour), and make sure anything
  full-stage one player opens is handled as a private screen in co-op ([coop-and-players.md](coop-and-players.md)).
- A screen with `handlesInput` loaded above another swallows every key for its player (the root movie
  stops at the first match). The MOD widget once blocked Start this way.
- Pushing filters in game requires AptViewer +0x3A8 (set by `LiveModeInitState::Reset`); forgetting it
  lets A reach the game under menus.
- "call of undefined method" is normal in these movies (GC silent); `SIMMY_APT_WARN=1` shows them.
- Apt runs on the wall clock (`Timer`, `getTimer`) except in `SIMMY_DET` mode; UI timing differs between
  runs and machines.
- Debug: `SIMMY_UI_TRACE=1` (every command, dialog texts), `SIMMY_APT_TRACE=1` (bytecode), `SIMMY_APT_TEST`
  (load and summarise movies), `--qd-test` (quickdata strings).
