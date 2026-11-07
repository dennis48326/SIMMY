# UI bucket audit (ui.tsv, 1745 stubs)

Verdicts: `ui_verdicts.tsv`. Counts: PC-replaced 862, NA-unreachable 673, NA-platform 167, MISSING-H 2,
MISSING-M 38 (7 features), MISSING-L 2, UNSURE 1.

## MISSING, by priority

### H1. Buy catalogue: the locked-object text shows a raw tag (80177CA8)
- **Ported** (hB): `UIObjectBase::doUIQDStringSubstitution` in `src/game/uitargets.cpp`.
- GC: `UIObjectBase::doUIQDStringSubstitution` is one line: it calls `UIQDTarget::UIQDSubstitutionStrings`.
- Port: `PCTTarget::GetLocalizable` (src/game/pcttarget.cpp:578) calls the generated stub, so nothing is
  substituted. It was hit in play logs.
- Player sees: "Earn <unlock_aspiration_points> aspiration points to unlock this object!" on every locked
  object (the string is in quickdat.arc, in every language). Other `<...>` tags in PCT text are also left as is.
- Effort: S (one line in uibase, or call `UIQDTarget::UIQDSubstitutionStrings` at pcttarget.cpp:578).

### H2. Tattoos never appear on sims (8016C850)
- **Ported** (hB): `GetTattooTextureID` in `src/game/cassimstate.cpp`; the compositor passes 0, 1, 2.
- GC: `CasSimPartsS2C::GetTattooTextureID(type, index)` picks the tattoo texture for location `type` (0..2)
  from `GetTattoosForLocation`. `SkinCompositor::GetTextureIDs` blends it into the skin.
- Port: the stub returns 0. It was hit in play logs. src/game/skincompositor.cpp:186 also passes
  `eTattooTextureTypeS2C{}` (type 0) for all three slots. GC passes 0, 1, 2. Fix both.
- Player sees: a tattoo picked in CAS shows in the CAS icon list but never on the sim's body.
- Effort: S. `GetTattoosForLocation`, `GetNumTattooTextures` and `GetGameStateTextureID` are already ported
  (cassimstate.cpp:282, simmodel.cpp:541).

### M1. Clothing and tattoo unlocks (CasClothingUnlocks, 21 functions 8015E9D4..8015F21C)
- GC: each player family has a byte per gender, part and style (2 = locked, 4 = new). `ClearUnlocks`
  locks everything, then runs `UnlockGroup(0)`. `TryCompleteGoal` kind 1 runs `UnlockGroup(n)`.
  `CasSimRendererDynamic::GetIndexes` returns the flags, so CAS shows locks and "new" marks.
  `CASSelectionTarget::ClearRecentlyUnlockedBitFlag` clears "new".
- Port: `GetIndexes` returns flags 0 (cassimstate.cpp:909-930). Fashion goals log a stub
  (objectsim.cpp:2957). `ClearRecentlyUnlocked` only clears the local copy (cassubtargets.cpp:1046).
  The bytes are streamed (family.cpp:277) but never read.
- Player sees: every clothing and tattoo style is open from the start. Fashion rewards unlock nothing.
- Effort: M. Note: the port initialises the bytes to 2 and never runs `UnlockGroup(0)`. Once the flags are
  read, port-made saves would show everything locked unless group 0 is unlocked at load.

### M2. Button-code cheats (PlayerCheats 80005D40-80005FFC, UpdateCheats 80005448)
- GC: each frame `ESimsApp::UpdateCheats` gives pad-1 presses to `PlayerCheats::Capture`. It matches
  11 six-button sequences (`EGlobal::InitPlayerCheats` 8004246C) and toggles a bit with a sound
  (`TogglePlayerCheat` 80042388). `GetPlayerCheats` is read by trees as simulator globals 0x2E/0x2F.
  `CRDTarget` reads `IsPlayerCheatOn`.
- Port: `UpdateCheats` is a stub (hit). simulator.cpp:588 forces globals 0x2E/0x2F to 0.
- Player sees: the game's documented in-game cheat codes do nothing.
- Effort: M. Capture, Init, Toggle, GetPlayerCheats and the two globals. The global.cpp pieces are in
  another bucket.

### M3. Fire-code meter is always empty (80057A1C)
- GC: `GetFireCodeValue(true)` calls `EMemoryMeterWin::PollMemoryLevels`. That counts objects through
  `EIObjectMan::CountObjects` (plus a term from 0x80475E94+0x20). The result is
  max + 0.3*mid + 0.1*min of objects/300, other/500, (memory-420K)/1190K, clamped to 0..1.
- Port: `GetFireCodeValue` returns 0 (pcttarget.cpp:154). Only `SIMMY_FIRE_CODE` changes it.
- Player sees: the buy/build FCM bar never fills. The "fire code" warning and the placement limit never
  happen. Lots can grow past the GC limit.
- Effort: M. It needs `EIObjectMan::CountObjects`, which is world side. Decide whether PC should keep the
  limit: it is gameplay, not just memory.

### M4. CAS camera zoom on the C-stick (801753B0, 801753EC; comment-only gap)
- GC: in `CasScene::UpdateCamera` (801665A8), once a camera move ends, the right stick moves `m_stickT`
  between the spot and its alternative spot, with motion blur. The result is stored in, and read back
  from, `_gCasTweakTool` (`SetCamera`/`GetCamera`).
- Port: there is only a comment at src/game/cas.cpp:681. `m_stickT` exists but nothing moves it.
- Player sees: no zoom in Create-A-Sim.
- Effort: S.

### M5. Description repair: CasSimDescriptionS2C::Verify (80168408, 80169468, 80169578, 8016977C, 8016D730)
- GC: `Verify` (8016850C, F5C bytes) clamps body type, skin, part, texture and style indices. It sets
  `m_bHairBottomDisplayed` from the hair model and recomputes inner/mid layer torso styles. It runs from
  `SimModel::Init`, `SAnimator2::wearNormal`/`setNewModel`, CAS change-sim and genetics defaults.
- Port: the whole function is an inline `SIMMY_STUB` (simmodel.cpp:343). It was hit. The listed helpers
  are its callees.
- Player sees: only when a description is out of range or inconsistent. Then you get missing or clipping
  parts, or a wrong hair bottom or sleeve style, after an outfit change, after randomising in CAS, or with
  older saves. How often this happens is unknown.
- Effort: M.

### M6. Belt shows through tops (8016CF78, 8016CFF8)
- GC: `SimModel::UpdateBelt` hides the belt when the mid or outer top's style matrix says it covers it
  (female/male tables).
- Port: an inline stub (simmodel.cpp:1455). The belt is always built.
- Player sees: belts clip through jackets and long tops.
- Effort: S. Two table lookups; the tables are in the GC data.

### M7. No vertex welding between body parts (8016D560)
- GC: `ChangeModelPart` asks `GetWeldableBodyPartConnections` for up to 7 neighbour parts and calls
  `WeldSharedVertices` for each.
- Port: an inline stub (simmodel.cpp:837). `WeldSharedVertices` (80071558) is not ported either.
- Player sees: possible lighting or geometry seams at the neck, wrists and waist after part swaps or
  morphs.
- Effort: M.

### L1. CAS screen unload check (80180068)
- GC: `CASTarget::UnloadLastScreen` does not unload again when the screen is already marked for
  unloading.
- Port: castarget.cpp:593 skips that check, so a second unload request can be sent. It is probably
  harmless.
- Effort: S.

### L2. Relationship list tie-break (801DA670)
- GC: `sort_records_by_name` compares first names, then last names.
- Port: r2ltarget.cpp:333 compares first names only, so sims with the same first name can appear in a
  different order.
- Effort: S.

### UNSURE: CasSimDescriptionS2C::ReadOldDescription (80167B7C)
- It was hit in a real run: some stream held a description without the 0xFEED mark. The port returns
  after reading the 2-byte mark (simmodel.cpp:229). GC reads the old layout. Anything streamed after it
  (costumes list, user data, animator state) is then misaligned.
- To run: print a backtrace and the resource or save name at simmodel.cpp:229 in the run that logged it.
  If it is disc data (default.NGH user sims, costumes), it becomes MISSING-H, effort M (the layout is in
  the raw function). If it is a port-written save, look for the writer bug instead.

## Non-MISSING verdicts

**NA-platform (167).** EyeToy:
- `EyeToyClient` (all 63, including the logged `Init`/`LoadDefaultSettings` and the save header/texture
  records).
- `E2ETarget` (screen 0x24) and the Urbz PDA `EYETarget` with its `GetVar_*`/`SetVar_*` handlers.
- `CasSimRenderer::UpdateRepShaders` (EyeToy rep slots).

Also: PS2 HDD save size, the GC DVD error string and language lookup, and the GC hold-to-reset combo
(`UpdateReset`).

**NA-unreachable (673).** Mostly code that nothing in the release build creates:
- `targets_tsc3` (238 + 18): the Urbz PDA family (PDATarget, INV, FON, GOL, KEY, PSE, REL, RGT, SKN,
  MSG; `StartFire`). Flows 0xB-0x14 come from no movie. OPTTarget: flow 0x16, its movie is not on the
  disc.
- `DlgWrapper` and `MUWrapper`: `MUWrapperStartup` has no caller.
- Dev tools: `ECheats` (dev cheat file and debug menu; `ECheats::Update` is an empty function),
  `CasNpcEditor` (needs dev flags), CTGDump, CAS costume/grandparent writers, `UIHDTV`.
- Classes never constructed: CasSceneGamecube/Personal, ScrollingTextBox, FlashPiMenu, PZ1Target.
- Debug-flag drawing, version overlay, HUD primitives 0x39-0x3C (debug object only), the Urbz job HUD,
  and accessors with no caller.
- In libaptz: script classes no disc movie names (Date, Sound, LoadVars, Stage, the Error class, and
  methods such as `sortOn`, `isToggled`, `getAnalogStickInfo`, `loadVariables`, `getBytes*`).
  Also the Apt debug, profiling, mouse and extension API.

**PC-replaced (862).**
- 745 libaptz runtime internals: values, GC, pools, display lists, loader and linker, CIH, action queue,
  input focus, script functions, Math/String/Array/Color/TextFormat/XML/Key/MovieClip methods. The PC
  runtime covers these in its own structure.
- How this was checked: every builtin method name of the GC runtime (sMethod_* and cbCallMethod_*) that
  appears in flashes.arc exists in the PC builtins. All 378 `TLA_name` command strings in the movies are
  either installed by the port or absent from the GC binary too. So no silently failing script call was
  found at the name level.
- Game side:
  - Static-init thunks.
  - Apt aux callbacks and button-filter helpers (targets call AptViewer directly).
  - ActionQueueHUD (done by ACTTarget).
  - NamespaceSelector and XObjLang (read directly from quickdata).
  - IconGroup balloons, PCT/CSP helpers and comparators (inlined).
  - Recon, synchronous resource flush, app shutdown, profiler hooks, and the EMemoryMeterWin shell around
    `PollMemoryLevels`.
