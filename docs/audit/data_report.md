# Bucket `data`: unported-code audit

2677 stubs (engine/, base/, game data layer). Verdicts are in `data_verdicts.tsv`.

| Verdict | Count |
|---|---|
| NA-unreachable | 1407 |
| PC-replaced | 1174 |
| NA-platform | 57 |
| MISSING-H | 1 |
| MISSING-M | 6 (3 features) |
| MISSING-L | 31 (10 features; 21 rows are the ERTQuantize4D class) |
| UNSURE | 1 |

Method: each stub's GC callers come from `graph.tsv`. I walked up chains of stub-only callers to find a live root (a ported caller, a data reference or a logged hit). Then I grepped the port source (`src/`, `include/`) for each method name and for each address. For anything marked MISSING, or where the name was not enough, I read the raw GC code and the port caller.

## MISSING, by priority

### 1. MISSING-H: in-game sim heads are never morphed (face sliders, head shape)
- GC: `SimModel::UpdateReweld` (80072CBC) runs after `ApplyAllMorphTargets`. It calls `RebuildModified`. If the head part (index 1) has a model and is not shared (flag 4), it calls `ERModel::ApplyMorph` on the head and sets flag 2. Then it calls `SimRenderer::MorphParts(parts, false)` and `DeallocateMorphResources`.
- Port: the stub is called from `src/game/esim.cpp:254` (initModel) and `:395` (ChangeCostume), and it shows up as a hit in real runs. If the head has face morph targets, `ApplyAllMorphTargets` (`simmodel.cpp:950`) returns without `ApplyMorph`, exactly as on GC. In-game drawing then calls `MorphParts(..., morphHead=false)` (`esim.cpp:963`, `:1033`), which skips part 1. So the head's `m_morphDirty` is never applied, and it keeps the base display list (`e_submodelshader.cpp:416`).
- Player sees: in live mode, every sim's face looks like the base head for its style. CAS face sliders (chin, nose, eyes, mouth regions) and the head-lattice "fat/thin" shape are lost. CAS itself looks right because it draws with `morphHead=true`.
- Effort: S. Port the 10-line function. `SimModel::m_partFlags[1] & kShared` is flag 4 and `kChanged` is flag 2.

### 2. MISSING-M: per-letter text effects (`ERFont::ProcessFontFX`, 8031702C)
- GC: a 0x6E0 function that changes each glyph's matrix by its fx byte. Rumble (1), wave (2), bulge (3), tipsy (4) and reveal (5) prerotate or prescale; fx 6 (outline) prescales by 1.4; fx 7/8 (rock and rotate) prerotate by the short in bytes 2-3.
- Port: `e_rfont.cpp:242` and `:246` call the stub whenever the fx byte is non-zero. The fx bytes are filled correctly by `aptstring.cpp` (AptFormat::Draw).
- Player sees: the disc's quickdata (in-game text) uses `<ROCK_>` in 95 strings, `<OUTLINE_>` in 3 and `<RUMBLE_>` in 1. That text does not rock or shake. Outlines are drawn at the same size as the text, so they hide behind it and no outline shows. Wave, bulge, tipsy and reveal tags are not on the disc.
- Effort: S/M (one function, pure matrix math).

### 3. MISSING-M: welding body parts after a part change (`SimModel::WeldSharedVertices` 80071558, `ERModel::WeldSharedVerts` 80319350, `GetWeldPos` 80319214, `SetWeldPos` 803192C0)
- GC: `SimModel::ChangeModelPart` (ported) gets the part's weldable neighbours (`CasSimPartsS2C::GetWeldableBodyPartConnections`). For each neighbour, `WeldSharedVerts` (tolerance 0.0001) makes shared border vertices and normals match, then sets flag 2. `ERModel::RebuildAfterWelding` is empty in GC.
- Port: `simmodel.cpp:838` has an inline `SIMMY_STUB` "welding shared vertices", which shows up as a hit in runs.
- Player sees: after CAS or wardrobe part changes, there can be lighting seams or hairline gaps where parts meet: neck/torso, torso/legs, sleeves/hands. These are clearest on morphed (fat or fit) bodies.
- Effort: M (WeldSharedVerts is 0xB34 bytes, plus the weld-position helpers and the connection table).

### 4. MISSING-M: wall lighting from both wall ends (`EInstance::CalcLights4`, 8022907C)
- GC: `EIWallPart::Draw` (8002A4C0) calls `CalcLights4` at the wall's first point, then `ERLevel::GetLightsAtLocation` at the second point. It merges the two sets: point lights with intensity under 0.01 or colour under epsilon are dropped, duplicates are removed, and at most 4 are kept.
- Port: `src/game/eroom.cpp:1008` uses `CalcLights` at the first point only, with a `// GC:` comment saying so.
- Player sees: a lamp near the far end of a long wall does not light that end. Wall shading differs from GC.
- Effort: S/M. `CalcLights4` itself is the same as the ported `CalcLights`; the merge loop in `EIWallPart::Draw` is the work.

### 5. MISSING-L items
- **Sim culling box** (`CalcTightBoundBox` 802EA53C, `CalcNodePos` 802F0428). GC `ESim::VisibilityTest` tests the box around the posed bone positions once the controller has matrices. The port (`esim.cpp:784`) tests a fixed box one tile across and 1.8 high. A sim whose limbs reach outside that box (lying down, reaching) could pop out at the screen edge. Effort S.
- **Sim visibility sphere** (`CalcVisibilitySphere` 802EFFCC). The port uses a fixed sphere centred at z 0.9 with radius 1 (`sanimator2.cpp:660`). This sphere is the sim's lighting and sort point. To check: whether sims on an upper floor get the right z (the sphere ignores the orient z). Effort S.
- **Camera cut animation start frame** (`GetTrackFrame` 802EF4DC, hit). `camera.cpp:1139` gets 0 instead of the main track's position after one `Update` (about one frame). Effort S (FindTrack, then `m_pos`).
- **Character cache** (`ECharacterManager::CanCache` 8032333C, hit). GC returns 1; the stub returns 0, so skeletons are freed at their last DelRef and reloaded. Performance and hitches only. Effort S.
- **InLevelBaseState::Shutdown** (80089EB0, hit). Resets 80485900/04 to 0 and 80485908/0C to 1 (`MUStateMachine::s_field90x`), deletes the HUD and clears 80475C10. The port deletes the HUD in livemode. I found no port reader that cares about the other globals today. Effort S.
- **EGraphics::DeselectTextures** (802F1034, hit at `e_graphics.cpp:82`). GC unbinds texture stages 0 and 1 at the end of every frame. No symptom seen. Effort S.
- **8-bit texture copies** (`CopyTexture32To8` 8030B228, `CopyTexture8To8` 8030B960, and the ERTQuantize4D class, 21 rows). Reachable from `SkinCompositor::FinalizeSkinTexture` and `TextureUtils::LoadTexture` only when the target is 8-bit. The skin is 32 bpp and these were never logged. Effort M.
- **UpdateReweldCAS** (80072D3C, hit, `castarget.cpp:390`). CAS draws with `MorphParts(true)` every frame, so only the morph-memory release is lost. Effort S (with item 1).

### UNSURE
- `EShader::ChangeMaterial` (802FA390). `ENgcShader::ChangeMaterial` (`e_ngcshader.cpp:30`) forwards to this stub. GC copies 12 material words and calls a vtable update. It was never logged. To decide: run a long live-mode soak with many object types, then grep the log for 802FA390.

## Non-MISSING verdicts

**PC-replaced (1174).**
- String, container and allocator families: BString/BString2, EString, EString2, EFixedString, StringBuffer, ea_printf, the no-case string tables, EAGeneralAllocator, pools, alloc groups, frame and scratch allocators, EBitArray/BitArray, hash and red-black trees, node lists, `_c2DArray` name registry (nothing reads it).
- Threads, mutexes, message queues.
- File and stream I/O, and the whole resource-file data layer: IFF, Chain, Quick, Ngh, Std, Obj and Seq res files, string sets, float constants, smart data factory, XML parsers, CTG files, ESimsDataManager (the port builds sim models synchronously).
- The resource managers and their stream, old-version and copy loaders. The port loads every type through EFile and AddRefs on demand, so `AddRefAll` preloads are not needed.
- The GC platform layer: THP threads, AX streams, ARAM, perf, XFB, pads, engine bring-up.
- Renderer, model, font and math pieces: the port records its own 16-byte EDL entries, and the vtable overloads it does not use were never logged. Model ENCamera/ENDummy records are kept raw. Bound and matrix helpers were inlined into ported callers.
- EStream `Read`/`Write` serialisation of levels, instances and lights (levels load through `ERLevel::Load(EFile)`).
- Trivial GC bodies the port does not need: `VerifyRequiredModels`, `NeedFlushToDestroy`, `GXInvalidateTexAll`, the empty `RebuildAfterWelding`.

**NA-unreachable (1407).** 990 are dead in GC (no call and no data reference). Another 250 are only called from dead code. Classes that are never constructed: EIPointAmbLight, BitArray64/BitMatrix64, EBoundTreeNode, the Seq/Std/Obj/Chain res files, and others. The rest are dev and debug tools (debug and cheat menus, ETweak/EConfig, sim logging, trace, timing display, system font, TGA writer), effects sequencers (`effectss.arc` on the disc is 16 bytes, empty), zodiac helpers and sepia portraits (every GC call passes `false`). It also covers behaviour that does nothing in the release build:
- **EGraphics bloom/motion blur/tint setters, `ERC::ProcessFrameEffects` and `ENgcRenderer::ProcessFrameEffects`.** These are hit in runs, but the only consumer, `ENgcRenderer::ProcessFrameEffects` (80349F30), is an empty function in GC. The visible frame effects go through FrameEffectsManager, which is ported.
- **`EAnimController::AddEventRange` and `ProcessEvents`.** Release UpdateTrack calls ProcessEvents with `false`, so no event is ever collected and Update throws the list away. The known-gaps entry "animation events of phase-locked tracks (EAnimController::AddEventRange)" is therefore not a real gap.
- **Two values that are computed and then dropped:** `ERLevel::CalcBounds` (BuildHouse keeps the result in a local it never uses), and the `EIObjectMan::Init` bound-rect display list (built and freed, never drawn).
- **`ENgcRenderer::GEList` and `ENgcRenderer::CopyScreenToTexture`.** Both only clear a one-shot warning flag in GC.
- **The unsmoothed-camera `LineCircleIntersect`.**

**NA-platform (57).** EyeToyManager and the EyeToy image-processing filters, `ESim::UpdateRepShaders` (EyeToy representation shaders), the HDD thread (PS2/Xbox), GC memory-card icon palette helpers, and the HDTV Apt projection (`SetOrthoProjectionForUIHD`).

## Things worth knowing
- `docs/known-gaps.md` lists "animation events (AddEventRange)" as a gap, but the release build discards those events (see above). It does not list item 1 (in-game faces not morphed) or item 4 (wall lighting).
- The bloom, blur and tint stubs that are hit in runs are harmless. Their only consumer is empty in the GC release build.
