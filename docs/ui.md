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

Saving from the game: the Save/Quit tab of O2T. "Quit" opens M2M with source 7: when
`EGlobal::IsSaveGameEnabled` (80042718: the HUD is not in a job mode, HUD +0xC8, and no player's camera
director holds a shot, state 1 with +0x234 set) and no player is in social mode
(`EGlobal::IsPlayerInSocialMode` 800409D8) and simulator global 0x18 is clear, it asks "save before you
quit?" (yes: the slot list, the save, then the main menu); otherwise "the game cannot be saved at this
time". "Save" refuses in social mode ("option_social_no_save") or with global 0x18 set
("option_generic_no_save") in the help dialog. The pause menu's build/buy status (`PAZBase::GetVariable`)
is "0" while the trees lock build/buy or a player is in social mode.

## The HUD ([`hudtarget.hpp`](../include/simmy/classes/hudtarget.hpp))

`HUDTarget` (`_gd.m_hudTarget`, made by `LiveModeInitState` / the "ingame" flow) serves `HUD_*`
variables, the clock in UIDB, pause counting (`IncPause`/`ReleasePause`), the action menus
(`ActionMenu m_menus[2]`) and makes `H2DTarget` (`_gd.m_h2dTarget`). `H2DTarget` owns per player the widget
targets `m_mod`, `m_csm`, `m_mot`, `m_skl`, `m_waf`, `m_act`, `m_int` (arrays of 2), runs them in a fixed
per-frame order (side panels every sixth update), and handles the d-pad side panels (left motives, up
skills, right wants and fears, down closes), Start (`H2D_PAUSE`: sets `PlayerNumber`,
`Player1FirstName`... from the **selected** sim's neighbour; refused while the player's camera director letterboxes a shot, `+0x234`, e.g. the lot's intro camera, as on GC), B cancel (`UpdateCancelButton`),
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

**Dialog icons** (`EdithDialogPrimitive::SetDialog` 800B56DC): bits 1-3 of the primitive's byte 7 pick an
icon, byte 1 its argument: 2 the portrait of the neighbour whose id is in that local of the tree
(`FindNeighborByID` -> its selector -> `ObjSelector::GetBigThumbnail`: a story sim's big portrait from the
"RelationshipTable", else the sim's thumbnail), 3 the thumbnail of the object whose id is in that local, 4
an entry of the UI strings' `DialogIconTable` (shader id, then two sizes; only the debug object uses it).
Icons are 64 x 64 unless the table says otherwise; G2D (modal) and MDI (modeless) show them. 164 disc
dialogs use kind 2 (phone calls, visitors, move-in, the story's reminders and villains), 54 kind 3.

**The lot name** (`HUD_lot_name`, `HUDTarget::GetPlayerLocalizable` 80187744): the current house's
"HouseData" text (`EHouse::GetHouseName` -> `EGlobal::HouseNameText`), or, while a family is just moving in
(DAT_80475F24), that of the house being entered (DAT_80476230). No disc movie was seen asking for it (lot
loads, pause, story and free play), like the reputation texts next to it (Urbz left-overs).

**Family names** (`FamilyImpl::DoStream` 800B6548): a family name read from the neighbourhood that is a key
("#...") is cleaned up: free-play houses 13-16 just lose the "#" (`EGlobal::GetNghFamilyName` 8004032C),
other houses take the "HouseData" row of that name, else lose the "#" (`GetNghHouseName` 80040414). The
disc's keys are "#house06" and "#freeplay1"; "HouseData" rows are named "house 06", so on GC too they
become "house06" / "freeplay1".

## Text effects ([`aptstring.cpp`](../src/game/aptstring.cpp), [`e_rfont.cpp`](../src/engine/e_rfont.cpp))

Text drawn through `AptParagraph` (dialog boxes, hints, HUD texts with embedded codes) turns codes such
as `<COLOR_..>`, `<FONT_..>`, `<PULSE_..>`, `<BLINK_..>`, `<PARTICLE_..>` into formats (`AptFormat`). The
per-letter effects are a 4-byte command (kind + 3 arguments) that `AptFormat::Draw` animates from the
system clock (`_sysclock`, the wall clock or SIMMY_DET's virtual clock; UI only, outside the sim hash) and
`ERFont::ProcessFontFX` (8031702C) applies to each glyph's matrix while drawing (on a copy of the command
per draw, so rumble and tipsy change it glyph by glyph):

| Code | Kind | Effect |
|---|---|---|
| `<RUMBLE_rrAA>` | 1 | jitter of up to AA/25600 and +-pi/32 turn from a rand stream seeded by byte 1 (re-seeded rr times a second); one glyph in ten 1.3x. PC: the GC C-library rand on a local state |
| `<WAVE_aabb>` | 2 | glyphs move up and down along the word |
| `<BULGE_aabb>` | 3 | the glyphs near a moving index grow |
| `<TIPSY>` | 4 | each glyph turned 45 degrees about a turning flat axis |
| `<REVEAL_aabb>` | 5 | glyphs from a moving index on are hidden |
| `<OUTLINE_RRGGBB>` | 6 | the word drawn twice in the colour, 1.4x, 2.5 px apart, behind the text |
| `<ROCK_L_ssaa>` / `<ROCK_W_ssaa>` | 7 / 8 | glyphs (L) or the word (W) rock by +-aa degrees at speed ss |

On the disc: `<ROCK_` in 96 strings (job menu entries and party/district texts that look like left-overs,
the development and author menu titles), `<OUTLINE_` and `<RUMBLE_` only in a DEBUG string, `<REVEAL_`
in two Japanese CAS prompts. `SIMMY_TEXT_FX_TEST=<code>:<word>` puts a code before a word to see them.

## Button-code cheats ([`appmain.cpp`](../src/game/appmain.cpp), [`global.cpp`](../src/game/global.cpp))

Every frame `ESimsApp::UpdateCheats` (80005448) gives a press on **player 1's pad** to
`PlayerCheats::Capture` (80005D40, `g_playerCheats`): a single button that some code uses joins a ring of
the last six presses (a wrong button, or a second since the last press, clears it first); a run of a
code's buttons toggles that cheat (`EGlobal::TogglePlayerCheat` 80042388: bit n of EGlobal +0x9C, with
the UI sound 0xCE619E32 on / 0x1C99B71C off while a game is loaded) and clears the ring. The codes are
`EGlobal::InitPlayerCheats` (8004246C, run by `SetDefaults`); the bits are cleared when the main menu
starts (`TheSimsMainMenuState::Startup`). Trees read them as simulator globals 0x2E and 0x2F
(`GetPlayerCheats` 80042338: both the same, every bit with the config's all-cheats switch +0x400).
With `p1_input=keyboard_mouse` the keyboard is pad 1, so the default keys enter them too (L q, R e, Z c,
X f, Y tab, A space, B backspace, d-pad the arrow keys). `[cheat] code n on/off` is logged.

| # | Buttons (GC pad) | Effect (tree / reader) |
|---|---|---|
| 0 | L, R, Up, A, Z | the **cheat gnome** (`zz_move_a_tron`, on every lot, hidden: data 0x22) appears ("Cheat Code - Show Gnome": any of cheats 0-2) |
| 1 | R, L, Z, Right, Left | gnome; its money interactions ("Cheat - Money", "Cheat Code - Give Money") |
| 2 | Up, X, Up, Right, Z | gnome; motives (fill, lower, alter all motives) |
| 3 | Z, X, Down, Left, Up | unlock all objects, give skill objects |
| 4 | X, Z, Left, X, Up, X | unlock / lock all lots |
| 5 | B, Z, Down, Right, B | unlock / re-lock all socials, unlock clothing |
| 6 | X, B, L, Up, Down | time (set the hour, advance) |
| 7 | Y, X, B, Z, Left | change skills |
| 8 | Z, B, Up, Down, Right, A | add aspiration, unlock recipes |
| 9 | Right, Down, Right, Down, Right | the credits' photo (`CRDTarget::Update`: slides across while on the credits screen) |
| 10 | R, L, R, L, Y | only a sound (0x3EA0941F); no bit (GC). The gnome's power-social items (flag 11) and reputation/relationship entries (flag 12) have no code |

Cheats 3-8 are offered through the gnome, so they need cheat 0, 1 or 2 too. Det replays contain no code,
so the det hash is unchanged.

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
