# Create-a-Sim, the neighbourhood, free play, lot changes

## Overview

Every sim the game knows is a `Neighbor` in the neighbourhood (`Globs_pNeighborhood`,
`NeighborhoodImpl`, [`neighborhood.cpp`](../src/game/neighborhood.cpp)); families own members by GUID and
live in a house (a lot, 1..16). The neighbourhood is read from `DEFAULT.NGH` for a new game and from the
memory card save on Continue, and written back whenever the game saves ([files-and-saves.md](files-and-saves.md)).
Create-a-Sim edits a sim's look and personality and writes it into a neighbour. Only the lot being
played is loaded as objects; other lots are changed in their saved form (free play's move-in, evict,
bulldoze).

## Data model ([`neighborhood.hpp`](../include/simmy/classes/neighborhood.hpp))

| Class | Saved as | Notes |
|---|---|---|
| `NeighborhoodImpl` | `NGBH` | Variables (`m_vars`: house being played, player ids, story state), families, neighbours by id-1 |
| `Neighbor` ([`neighbor.cpp`](../src/game/neighbor.cpp)) | `NBRS` | `m_pers[0x50]` person data, motives, GUID, `m_selector` (its `ObjSelector`), `RelMatrixImpl`, 20 want/fear bookmarks + 7 active slots |
| `FamilyImpl` / `PlayerFamilyImpl` ([`family.cpp`](../src/game/family.cpp)) | `FAMI` (id, `\|0x4000` for a player family) | Members (GUIDs), house (-1 moving in), funds, flags (0x10 P1 done CAS, `kP2DoneCAS`); player family: inventory, goals/unlocks (`GoalUnlock`), clothing |
| A CAS-made sim's look | `User` (`UserDataSaveLoad`: GUID + `CasSimDescriptionS2C`) | On a "user" `ObjSelector` (`m_simDesc`, `m_userName`); `ObjectFolderImpl::LoadUserData/SaveUserData` |
| Portraits | `thmb` | `ThumbnailLoader` ([`thumbnail.cpp`](../src/game/thumbnail.cpp)): 32x32 C8 + palette |

Tuning: `NeighborhoodConstants::UpdateConstants` (800CC740) reads group 4 of the global object file's
float constants at the first load (architecture value x0.7, new family money 20,000, start hour, friend
count, house score weights; the code's defaults when a name is missing). `NeighborhoodImpl::Save` puts the
played house's number in variable 2, gives the played family its house value from the simulator (objects
+ architecture x0.7 + lot) and brings each family's friend count up to date before writing it.
`AddToFamily` / `RemoveFromFamily` also give the person on the lot its family number (person data 0x3D)
and redo the visitor types ([simulation.md](simulation.md)).

Lookups for other code: the player family is `Globs_pNeighborhood->GetFamily(_gd.m_fieldD8)` (not
`GetFamilyInHouse`); `NeighborhoodImpl::SimDescriptionOf(guid)` gives a sim's look;
`_gd.m_players[p].GetPrimaryNeighbor()` a player's own sim; `PersonNeighbor(person)` the neighbour of a
person on the lot.

## Game start ([`pregame.cpp`](../src/game/pregame.cpp))

`PreGameState` (state 8): a new story game goes to player 1's CAS (`PreGameCasPlayer1State`, state 10:
family 1 member 0 "Chris" is the slot), sets family 1's flag 0x10 and house -1; continuing skips CAS when
family 1 has 0x10. Two players: player 1's CAS, then `InsertPlayer2Roommate` (800948D4: family 2's first
member moves in as a roommate) and player 2's CAS (state 11). `GameData::PlayerEnterHouse` picks the lot
(story start: 9); `GameData::GotoLiveMode` -> `LiveModeInitState`. Free play: `PreGameFreeplayState`
(state 9, `FAMTarget`).

## Create-a-Sim

| Piece | File |
|---|---|
| `CASTarget` (the CAS UI's game side, `_gd.m_casTarget`), its nav commands, Done -> `SaveCASDataToNeighborhood` | [`castarget.cpp`](../src/game/castarget.cpp) |
| `CasMediator`, `CasScene` (room, lights, camera, mirror), `CasSimRenderer(Dynamic)` | [`cas.cpp`](../src/game/cas.cpp) |
| `CasSimState` (the description being edited), idles, zodiac | [`cassimstate.cpp`](../src/game/cassimstate.cpp), [`ezodiac.cpp`](../src/game/ezodiac.cpp) |
| Sub-targets: Body, Misc, Fashion, Genetics (+ parent portraits), Selection, Tattoo, Morph, Personal, Roommate | [`cassubtargets.cpp`](../src/game/cassubtargets.cpp) |
| Name keyboard (`K2YTarget`) | [`k2ytarget.cpp`](../src/game/k2ytarget.cpp) |
| Morphs (`BSplineVolume` lattices, morph targets, `MorphUtilities`) | [`bsplinevolume.cpp`](../src/engine/bsplinevolume.cpp), [`e_rmodel.cpp`](../src/engine/e_rmodel.cpp), [`morphutilities.cpp`](../src/game/morphutilities.cpp) |
| The CAS states (`CreateASimBaseState`, auto-accept) | pregame.cpp |

`CAS_mode` (UIDB): 0 normal (story / new game), 1 wardrobe (in-game), 3 Create-a-Family; the mediator's
own mode word is what `CASTarget::m_player`/`m_mode` refer to (they are references to the mediator's
fields, as on GC). Done path: `SaveSimToFamilySlot` -> `GameData::ReplaceSimData` ->
`Neighbor::LoadFromCharacter` (personality, look into the user selector, portrait) ->
`NeighborhoodImpl::Save` -> the save prompt (M2M) -> live mode. After a declined save, `M2MTarget`
re-enables CAS input (`CASTarget::SetHandleInput(1)`). `SIMMY_CAS_ACCEPT=<frames>` auto-accepts CAS
(`SIMMY_CAS_ACCEPT_WARDROBE` for the wardrobe's): it runs `SaveCASDataToNeighborhood` as Done does, then
skips the memory card screens. (Before mL it skipped the save too, so a quick co-op start kept the disc's
placeholder sims: player 1 "Chris", player 2 "Pat", neighbour 60, whose face morphs are all 0 on the
disc. With the real Done path, or now the hook, player 2 is the sim made in its CAS.)

**Camera and turning** (`CasScene::UpdateCamera` 801665A8, `RepositionCamera` 801660D0, `InitCamera`
80166478, `CasSimRendererDynamic::Update` 8016FF08). A camera move stores its destination in the CAS
tweak tool (`_gCasTweakTool`, `CasTweakTool::GetCamera/SetCamera` 801753B0/801753EC, castweaktool.cpp);
each camera also has an alternative (`m_altCameraAngles` 803DFB0C, mostly camera 0's wide shot). At rest
the C-stick's y zooms between the two (C-down: out; smoothstep, motion blur while moving; let go, the
camera stays where the stick left it, read back from the tweak tool), and its x turns the sim (90
degrees a second at full deflection) unless rotation is locked. The pad is the CAS player's mapped pad,
else the active pad unless it is player 1's (`CasPad` in cas.cpp).

**Ambient score.** `CasScene::Init` (801653FC) starts an `AmbientScorePlayer` with one listener on the
room's score `scoreCAS` (`GetAmbientScoreName` 801672C8, `{anonymous}::SCORE_NAME`); `Update` advances it,
each focus change (`HandleEventChangeFocus` 80166BB0) moves the listener to the spot being looked at
(genetics, the roommates' second place, else the mirror spot; facing +y) and the scene's destructor stops
it. `CASTarget::UnloadLastScreen` (80193F2C) asks `UIScreenManager::IsUIScreenMarkedForUnloading`
(80180068), which on GC only reads a word before the record table for a screen without a record, so it
never refuses (kept as GC).

**Clothing and tattoo unlocks** (`CasClothingUnlocks`, 8015E9D4..8015F21C, casclothingunlocks.cpp; kept
at `PlayerFamilyImpl` +0xC8, streamed in `FAMI` from version 0x10). A byte per gender, body part and
style (2 x 32 x 40) and per gender, tattoo type and texture (2 x 8 x 30): flag 2 locked, flag 4 "new".
`ClearFamily` -> `ClearUnlocks` locks everything, then unlocks group 0 (each style's group is
`CasSimPartsS2C::GetUnlockGroupNum`; 1000: never offered). The disc's player families hold GC's starting
set (1726 open, 834 locked). `CasSimRendererDynamic::GetIndexes` (8016FD00/8016FE04) gives CAS the flags
of family 1's unlocks for the edited sim's gender: the selection grid shows a padlock on locked styles
(they cannot be picked) and an open padlock on new ones; picking a style clears its "new" mark in the
family too (`CASSelectionTarget::ClearRecentlyUnlockedBitFlag` 80199974). Fashion goals
(`TryCompleteGoal` kind 1, objectsim.cpp) unlock group n marked new: the car pool's "Unlocks - Career
Outfits" (career promotions, 61 sites), the global "Adjust Relationship" (group 6, with the reward
dialog), and cheat 5's gnome interaction "Cheat - Unlock - Clothing" (all groups). **PC save fix-up**
(`PlayerFamilyImpl::DoStream`): a family the port's old `ClearFamily` made (every byte 2) gets group 0;
any fashion goal already granted whose group still has a locked style gets that group (new). Neither can
happen in a GC-made state, so GC saves are untouched. `SIMMY_UNLOCK_CLOTHING=g1,g2` (dev aid) unlocks
groups in family 1 at load.

## A sim's look in the game ([`simmodel.cpp`](../src/game/simmodel.cpp), [`skincompositor.cpp`](../src/game/skincompositor.cpp))

`ESim::initModel` / `ChangeCostume` build the sim from its description as on GC: `SetAllModels`, the
lattice morphs (body, arms, legs, head shape), `ApplyAllMorphTargets` (the face sliders: each region off
its middle registers and weights its two head morph targets), then `SimModel::UpdateReweld(true)`
(80072CBC), which applies the head's pending morph (`ERModel::ApplyMorph`), morphs the other parts
(`SimRenderer::MorphParts(.., false)`) and frees the morph data (`DeallocateMorphResources`: the parts keep
their shapes and count as shared). CAS uses `UpdateReweldCAS` (80072D3C, `MorphParts(.., true)`).

Welding: `SimModel::ChangeModelPart` (8007115C) asks `CasSimPartsS2C::GetWeldableBodyPartConnections`
(8016D560) for up to 7 parts the new part meets (head-hair-hat, torso-sleeve-lower body, shoes-boot top,
...) and `WeldSharedVertices` (80071558) each pair that is loaded and not shared: `ERModel::WeldSharedVerts`
(80319350, tolerance 0.001 in this model's units, `GetWeldPos`/`SetWeldPos` 80319214/803192C0) moves the
vertices of both models that lie together to their mean and gives them the first model's mean normal
(as GC: the second model's normals are ignored). Only models with load flag 2 (weldable) take part. The
arrays are big-endian and drawn by reference, so `RebuildAfterWelding` stays empty as on GC; the result is
no lighting crease where parts meet (shoulders, neck, waist). `RebuildModified` (80072E30) clears the
changed marks.

`SimModel::UpdateBelt` (80073EDC) leaves the belt off when the mid top (`MidLayerTorsoHidesBelt`
8016CF78) or the outer top (`OuterLayerTorsoHidesBelt` 8016CFF8) hides it: the GC's four per-gender
tables (803E1403/803E1487/803E14F9/803E1527, by top style and its torso option; males never hide belt 2).
On the disc some neighbours have one (e.g. Bill: belt 8 under mid top 2, option 3).

`CasSimDescriptionS2C::Verify` (8016850C, with `VerifyBodyPartNoTexture`/`WithNoOptions`/`WithOneOption`
80169468/80169578/8016977C, `GetNumBodyPartOptions` 8016D730, `GetBodyPartName` 8016D820, `AssertWithInfo`
80168408) repairs a description when a sim is built, dressed or edited: indices, textures and options in
range and with models/textures, the hat-driven hair-bottom flag, the torso layers' styles, the boot-top
flag, tattoos, morphs (body 0..2, head 0..7, face 0..3), personality (0..10, else Aries'), aspiration
and names. GC passes the int options of hair, hat, lower body and arm accessory as `signed char&`, so on
the big-endian GC only their high byte is checked: kept. PC logs each repair as `[desc] Verify: ...`
(GC's assert text). No disc description needed one in the runs made. A description without the 0xFEED
mark is the older layout (`ReadOldDescription` 80167B7C): only the names and the gender are kept on a
cleared sim and the old look is read past (GC quirk kept: the first name's first character becomes
0xFEED). It was logged once in an old free-play run (lot 13) and not since (free play lot 13 entered
again, story start, saves); `SIMMY_RECON_TRACE=1` prints `[desc] old-layout description read` if it happens.

The skin
is composited per map quadrant (`SkinCompositor`): base skin, the quadrant's tattoo (locations 0, 1, 2 =
`CasSimDescriptionS2C::m_tattoos[0..2]` through `CasSimPartsS2C::GetTattooTextureID` 8016C850; the
game state uses the `_GAME` texture), then each part's texture.

## Portraits

`SimImageMaker` ([`simimagemaker.cpp`](../src/game/simimagemaker.cpp)) renders a sim's head into a render
surface and quantises it (`ERTQuantize`, [`e_rtquantize.cpp`](../src/base/e_rtquantize.cpp)) to a 32x32
C8 texture. **PC:** the draw happens at once on the app thread (as GC, with `EGraphics::Flush` so aurora
reads the vertex arrays before the caller frees the model), but the pixels come back through aurora's
asynchronous readback, so `CreateImage` returns the texture blank and `SimImageMaker::ServiceDeferredImages`
(each frame, from `ESimsApp::Update`) fills it in when the readback arrives. Headless there is no GPU:
it gives up after 600 frames and the portrait stays blank. `EGlobal::CreateThumbnail` / `ESim::CreateThumbnail`
render one on demand for a sim with no `thmb`. `SIMMY_PORTRAIT_DUMP=<prefix>` writes each finished one.

## Free play ([`famtarget.cpp`](../src/game/famtarget.cpp), [`hoodmanager.cpp`](../src/game/hoodmanager.cpp))

`FAMTarget` (screen 0x44): lots 13..16 ("Places to Go"), families 20..27, Enter House / Evict / Move In /
Bulldoze / Create-a-Family (state 12). A lot not being played is changed **without loading it**
(`HouseRecon`): its `objt` types are read, the types to remove are written as the house's `DUMP`
resource, `SIMI` lot values (and for a bulldoze the ground floor's floors and walls) changed, and the house
section written back to the card cache. When that house next loads, `ObjectModuleImpl::killDemolishedObjects`
(from `PostLoad`) kills objects of the dumped types. A new game copies all 16 houses into the card cache
first (`NghResFile::CopyDefaultNghToCache` 800D7C20), so these edits persist. Lot price = lot value +
objects + 0.7 x architecture (`NeighborhoodImpl::GetHousePrice`). New free-play families get
`gMoneyForNewFamily` (20,000) and pay the lot price on moving in.

## Leaving a lot (two GC paths, kept as two)

| Path | Trigger | Code |
|---|---|---|
| Flows 5 "Wardrobe" and 10 "Movie" | wardrobe's Change Clothes (generic sim call 0x22), the story's last lot (`TransitionToNewHouse` mode 4) | `InLevelBaseState::Update`'s `LeaveLotStage` ([`inlevel.cpp`](../src/game/inlevel.cpp)): HUD off, pause, dialog down, loading screen, funds committed, `SaveHouse` + memory card request 5, `KillAllObjects`, `SetFamilyToNull`, live mode killed; then in-game CAS (state 14) or the ending movie (state 15) |
| Taxi / car "Leave Lot" | `TransitionToNewHouse` modes 3/5 (generic sim call 0x16) | `ELiveMode::Update` save -> `ELiveMode::ChangeHouse` -> `LiveModeChangeHouseStateMachine` ([`changehouse.cpp`](../src/game/changehouse.cpp)): save (MU request 7), unload (`SetFamilyToNull`), demolish (`PlumbBob::ShutdownAll` + `KillAllObjects`), load the next house |

In-game CAS: `GenericSimCallWardrobe` (part of 80102A24, [`ingamecas.cpp`](../src/game/ingamecas.cpp)) sets
`CAS_mode` 1 and the neighbour; `InLevelCreateASimState` runs CAS and returns to live mode on the same
lot (reloaded). Food creation (generic sim call 0x27 -> `GameData::GotoXamMode` -> flow 0x19 `INGTarget`)
is in [ui.md](ui.md) and [coop-and-players.md](coop-and-players.md).

## Gotchas

- On lot 13 the Freebers start in chains of autonomous Talks; switching to a sim that runs "Be Talked
  With" leaves it waiting (GC behaviour: the running action is kept on a switch).
- Strings written by the port are GC-style (0xFFFE + big-endian UTF-16); `DEFAULT.NGH` has 0xFEFF
  little-endian; both are read.
- Test helpers: `SIMMY_TWO_PLAYER=1` (two CAS), `SIMMY_DEV_MENU=1` (main menu dev menu: any lot),
  `SIMMY_LEAVE_LOT_TEST`, `SIMMY_FAM_TRACE`, `SIMMY_PRISTINE_SAVE` ([testing.md](testing.md)).
