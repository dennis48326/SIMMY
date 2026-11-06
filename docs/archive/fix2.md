# fix2: story CAS genetics crash (A press) + CAS walk by pad (tree /home/claude/gfx/port, merge base /home/claude/base64/port)

Full diff (gen/ excluded): $S/fix2/session_full.diff. Before binary: $S/fix2/simmy_before.
Scripts (G=$S/fix2):
- walk.sh NAME LAST "PADSEQ" [args]  offscreen, SIMMY_UI_TRACE, SIMMY_PAD_SEQ_UI=1, a shot every EVERY (100) frames from FROM
  (1400); prints the presses; shots NAME_<frame>.png. (shot.sh / hd.sh as fix1's.)
- hlseq.sh NAME FRAMES "PADSEQ"  headless, UI trace, SIMMY_PAD_SEQ_UI=1.  gdbh.sh: headless under gdb (bt on a crash).
- asan_build.sh: ASAN build of this tree (from `ninja -t commands`; objects in build/asan, rebuilt by source mtime only -
  header changes need the object deleted), links build/simmy_asan against gcc's libasan.so.8 (clang's runtime is not
  installed). asanh.sh NAME FRAMES "PADSEQ": headless ASAN run (detect_leaks=0, halt_on_error=1).
- suite/: the B-cancel suite (w19 recipe) on this build.

## Pad recipes (SIMMY_PAD_SEQ_UI=1, no SIMMY_CAS_ACCEPT)
- Story CAS: "start,a,right,a,up,a,a" reaches CGE (genetics, focus 6 = generate). "a" generates, "right" = Done (item 5),
  "a" -> CBA (main: 0 Body, 1 Fashion, 2 Aspiration, 3 Personality, 4 Name, 5 Done; remembers the last option).
  Body (CBO): 0 Skin Tone (l/r), 1 Body Type (l/r), 2 Morph Body, 3 Morph Head, 4 Hair (select item), 5 Makeup, 6 Tattoo, 7 Done.
  Fashion: 0 Head (acc), 1 Tops (torso), 2 Bottoms, 3 Accessories, 4 Done. Item screens: a -> Select Item, a -> Fashion Design.
  Name: keyboard; its spatial list starts on Symbols (Space is drawn highlighted), so "right,right,a" = Enter (first ->
  last name), "right,right,a" again back to CBA. Done: "down x5,a" from a fresh CBA, then the save prompt "a" (Yes),
  "a" (M2M) -> ingame -> lot 9 (e2/e5 runs).
- Free play Create-a-Family: "start,start,right,a,right,a,up,a,a,down,down,a,a,down,down,a" -> CAR; "a" (+) -> CGE
  "a,right,a" -> CBA "down x5,a" -> CAR "right x3,a" (check) -> save "a,a" -> FAM "down,down,a,down,down,a,a" moves
  family 22 into house 15 and enters it (f2).
- Two players (SIMMY_TWO_PLAYER=1 SIMMY_STORY=0): player 1 as story, then "a,a,up,a" (save Yes, M2M, overwrite? Yes),
  player 2: "p2:a,p2:right,p2:a" (generate, done), ..., "p2:down x5,p2:a", then "p2:a,p2:a,p2:up,p2:a" for the save,
  then house: both sims on lot 9 (tp6).
- Wardrobe: story CAS by pad as above, SIMMY_STORY=0 SIMMY_SIM_START=18,27, then s23's wardrobe sequence after "house".
- The pad sequencer change is opt-in (SIMMY_PAD_SEQ_UI=1): without it the old behaviour is kept, and the B-cancel suite
  depends on it (its "left" repeats in CAS until SIMMY_CAS_ACCEPT, so its last "a" lands in the house).

## Bug 1: story CAS genetics A press crashes (the reported one; every new game by pad)
Two PC problems on the same path (CASGeneticsTarget::Update stages 1/2: CreateParentTexture, then delete m_parentModel,
then the next stage builds the other parent's model):
1. SimImageMaker::CreateImage (PC deferred design, s23) drew the portrait on the NEXT frame (ServiceDeferredImages) with
   the raw EIStaticModel pointers of the caller's model: the parent model was already deleted -> ERModel::ResetMorph on
   freed memory (gdb: SimRenderer::MorphParts <- SimRenderer::Render <- SimImageMaker::ServiceDeferredImages).
   GC draws inside CreateImage (8006EDE0). Fix: CreateImage/CreateImage32x32 draw at once on the app thread (DrawJob, the
   GC drawing), only the pixel readback stays deferred; off the app thread (_pApp->IsCallingThread false: no caller
   known) the draw still waits for ServiceDeferredImages.
2. With the draw at once, the parents' portraits came out as garbage close-ups and the next CAS draw crashed in
   ENgcRenderer::SetTexture (slot->Unlock on a destroyed texture):
   a. aurora frames are open during ESimsApp::Update (EEngine::PreFrameUpdate begins them), and aurora's FIFO worker
      reads GXSetArray data by pointer later; the model was freed (and the mom's built in its memory) before it did.
      GC's CreateImage calls _pGfx->Flush() right after SimRenderer::Render (and GC's FreeVertexData flushes before
      freeing), but EGraphics::Flush (802F11B0) was a generated stub. Ported it (app thread: Send the open immediate
      RC, _pSched->Flush -> ENgcRenderer::Flush -> GXDrawDone), and DrawJob calls it as GC. ENgcRenderer::Flush only
      calls GXDrawDone with the aurora frame open (draining outside a frame is not supported by aurora).
      Bisect that proved it: drawing the same job after UpdateApt / UpdateGame (model kept alive) was correct.
   b. The renderer kept the freed skin texture in m_textures[0]; GC's next SetTexture unlocked it through the freed
      object's base vtable (harmless there). ENgcTexture::~ENgcTexture now clears the renderer slots that hold it
      (as SetTexture(nullptr) would, m_numTextures kept consistent).
Result: genetics A generates, both parents' portraits correct (outputs/cas_walk_genetics.png), no crash offscreen, headless,
ASAN. The before binary crashes the same way as the old build (d_b run).

## Bug 2: back in CAS after a declined save, CAS ignores the pad (soft lock)
CASTarget::GotoGame (Done) does SetHandleInput(false). If the save does not go through (e.g. "No" on "overwrite your saved
game?" - the default in the two-player flow), M2M returns to CAS and CBA is restored, but nothing re-enabled input: Done and
every option press were ignored forever (tp3). GC: M2MTarget::~M2MTarget (801BFCDC) and SetCurrState state 10 (801C2A74)
call CASTarget::SetHandleInput(1) on DAT_80475BC0; the port had comments "create-a-sim is not ported". Ported both
(memcardscreens.cpp). tp4: after "No" Done works again and the save prompt comes back.

## Walked by pad (no SIMMY_CAS_ACCEPT), no crash or hang
Story: genetics (generate, done, back from main via B -> exit prompt -> main menu), body (skin, type, morph body sliders,
morph head, hair select item -> fashion design, makeup, tattoo), fashion (head/torso/bottoms/accessories -> select item),
aspiration, personality (zodiac, traits), name keyboard (typing, backspace, Enter first/last), Done, save prompt Yes/No,
live mode lot 9. Free-play Create-a-Family through storing, saving, moving in and entering house 15. Two players: both
CAS, both saves, both sims on lot 9 (headless, ASAN for player 1/2 CAS). Wardrobe CAS: Fashion -> Torso -> Select Item ->
Fashion Design -> Done chain. ASAN headless: story genetics/generate/body walk, two-player CAS: no reports.
Shots: /mnt/user-data/outputs/cas_walk_*.png (cas_walk_sheet.png labelled; 01..16 single screens; 17 = the two-player
wide split on lot 9 from the split regression run, which uses SIMMY_CAS_ACCEPT=300; the two-player pad CAS runs were
headless).

## Regression checks (final binary unless noted)
- B-cancel suite (suite/, SIMMY_FUNDS=1000 SIMMY_STORY=0; binary just before the last portrait-only refactor, AppThread -> _pApp->IsCallingThread): toilet bladder 56.9, sofa comfort 28.3, shower hygiene 45.6,
  bed energy 31.8 (sleeping), "cancels: 1" - as fix1 (57.9/28.3/46.4/32.7/1). Story start (lot 9) in every suite run.
- Split (SIMMY_TWO_PLAYER=1 --split=wide, SIMMY_CAS_ACCEPT=300): both views, both sims (splitw.png).
- ASAN headless (simmy_asan, final code): story genetics generate + body/morph/hair/fashion/name screens; two-player
  CAS runs (tp1/tp2, previous code): no reports.

## Noted, not changed
- Name keyboard: the spatial list's selection starts on Symbols while Space is drawn highlighted (first "right" goes to
  Space, the second to Enter). Not investigated further (Apt runtime / movie init order).
- The pad sequencer cannot see Apt-internal slider/list changes; with SIMMY_PAD_SEQ_UI=1 presses in CAS are not repeated.

## Files vs base64 (all hunks in session_full.diff)
- src/game/simimagemaker.cpp (port-owned, s23): 4 hunks (DrawJob: the GC drawing + _pGfx->Flush; NewJob draws at once
  when _pApp->IsCallingThread(); ServiceDeferredImages uses DrawJob only for a not-yet-drawn job; its comment)
- src/engine/e_graphics.cpp: 1 hunk (EGraphics::Flush 802F11B0 ported; the generated stub goes)
- src/engine/e_ngcrenderer.cpp: 1 hunk (ENgcRenderer::Flush: GXDrawDone only with the aurora frame open)
- src/engine/e_ngctexture.cpp: 1 hunk (~ENgcTexture leaves the renderer's texture slots)
- src/game/memcardscreens.cpp: 2 hunks (M2MTarget dtor and SetCurrState(10): CASTarget::SetHandleInput(true))
- src/game/uitargets.cpp: 1 hunk (dev aid: SetVariableCommandTable::ExecuteCommand reports the set to the pad sequencer)
- src/platform/pc_devtools.cpp: 5 hunks (dev aid: SIMMY_PAD_SEQ_UI, the UI-set counter, doc comment)
MSVC: no VLAs/extensions; free functions declared at global scope (extern ENgcRenderer _ngcRenderer, simmy_dev_note_ui_set);
std::map include added.
