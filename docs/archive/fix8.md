# fix8: sim switching (Winston -> Clarice, lot 13 Freebers, 1P)

Tree /home/claude/gfx/port (= base77 + this fix), build /home/claude/gfx_build. Diff vs base77: $S/fix8/fix8.diff
(1 file, 1 hunk). Before binary: $S/fix8/simmy_before. Scripts: hl.sh / os.sh (copies of fix7's, G=$S/fix8),
sheet.py, gsc.py (scans every BHAV for generic sim calls 0x1A/0x1B/0x1F/0x20), suite/ (fix7 suite pointed here).
Switching: Y = controller cmd 0xE -> interactor cmd 0xD (next sim). Cmd 0xD (previous) is mapped to button bit 1,
which no pad button produces (g_simsCtrlCommands, as on GC), so "back" is Y round the family.
Freebers: member 0 Winston #79, 1 Clarice #80 (starts in the kitchen, 19.5,12.5), 2 Cindy #81, 3 Horatio #82.

## 2. Pause pages show Winston (fixed)
- Only the name box was wrong. H2DTarget::SetVariable "H2D_PAUSE" (GC 801B7EB0) sets Player1FirstName /
  Player1LastName (shown as <first_name> <last_name> in every pause page header) from the selected person's
  neighbour: person vtable 0x314 -> +0x1C neighbour id -> Neighborhood vtable 0xC4 -> Neighbor +0xC selector.
  The port used `_gd.m_players[p].GetPrimaryNeighbor()` (the player's primary sim, Winston) instead.
- Fix (hudtarget.cpp): `PersonNeighbor(GetSelectedPerson(p))` (cXPersonImpl::m_neighbor).
- The page contents were already per selected sim: WXF (PlayerNeighbor = selected's neighbour), CSP and R2L
  (GetSelectedPerson), the HUD (SelectedPersonChanged). Checked with SIMMY_WANTS_TRACE: the WXF page lists the
  selected neighbour's slots (Winston "Make Out / Crush / Fashion", Clarice "Take out trash / Lover / Kiss").
  PRG (progress) uses player 1's primary neighbour on GC too (801D9604/801D97DC) - left as GC.

## 1. "Could not control Clarice, she stands in the kitchen doing nothing" (no port bug found)
- The switch path matches GC: DirectInteractor::OnCommandPressed 0xD (8020506C) -> EGlobal::AdvanceSelectedPerson
  (80040CC8) / SelectNextInFamily (80040AE0) -> SetSelectedPerson (800407DC: animator player control, HUD queue
  access, H2DTarget::SelectedPersonChanged 801B8BA4: motives, want icons, camera centre; global 3/0x20) -> the
  interactor's person, SnapToPos, cancel timer -> 0.5 s later CancelQueuedActions (80204F3C): routing ->
  CancelAllActions (vt 0xCC), else CancelAllButLastActions (vt 0xD4, 801262B0): the queued actions go, the
  running one stays. DirectInteractor::Update re-reads DAT_80475E88 every frame. No cached person elsewhere
  (camera, ActionMenu, ACT/CSM, autonomy all use GetSelectedPerson). GC/port compared function by function.
- Tested (headless + offscreen): after Y, Clarice walks with the stick, the camera, plumb bob, HUD portrait,
  motives and wants follow her, A opens her menu and the chosen action runs ("#80 queues Wash Hands ... runs ...
  route ... hygiene up"), pause round trip keeps her, Y round the family returns to Winston, B works.
- Reproduced what the user most likely saw: on lot 13 the Freebers start in a chain of autonomous Talks.
  Clarice (kitchen) often ends up running "Be Talked With" (priority 3), waiting in tree 20ac "SG_wait for
  Person A to be here or higher state" while her partner (#82) finishes another conversation (900-2000 frames;
  without input it resolves: #82 comes and they talk). Switching to her keeps that running action (GC
  CancelQueuedActions keeps the current one), the animator stays simulator-controlled (anim state 7), so the
  stick does nothing (UpdateControlStatus 8020450C: status 0 outside states 9..14) and there is no hourglass
  (GC shows it only for person data 0 == 3 or routing). B ("cancels: 1") or any chosen action (priority 100
  interrupts) gives her back at once. That is GC behaviour and GC data; nothing was changed for it.
- Unported but unused: generic sim calls 0x1F/0x20 (SetSelectedPerson to the stack person); gsc.py found no
  tree that calls them (0x1A/0x1B, ChangeSelectedSimL, are ported).

## Co-op (SIMMY_TWO_PLAYER=1, split=windows)
P1 Winston, P2 Clarice. P1 Y -> skips Clarice ("the other player's") -> Cindy; P2 Y -> skips Cindy -> Horatio.
P2 Start + A: player 2's WXF shows "Horatio Freebers" with the blue (P2) plumb bob (fix8_coop_p2_pause.png).

## Results
- Suite (suite/, this binary): toilet 56.7, sofa 28.3, fridge 32.0, shower 45.5, bed 31.8; 0 "script error"/
  TREEASSERT in s_*.log; `grep -c "cancels: 1" s_cancel.log` = 1.
- Outputs: /mnt/user-data/outputs/fix8_before.png (old binary: Clarice controlled, pause header "Winston
  Freebers"), fix8_clarice_after.png (Clarice controlled; wants page and career page headed "Clarice
  Freebers"), fix8_pause_winston_vs_clarice.png, fix8_coop_p2_pause.png.
