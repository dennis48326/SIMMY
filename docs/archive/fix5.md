# fix5: free play soft-lock after "Socialize" (lot 13, Freebers)

Tree /home/claude/gfx/port, build /home/claude/gfx/build, merge base /home/claude/base69/port (untouched).
Full diff (gen/ excluded): $S/fix5/session_full.diff. Binaries: $S/fix5/simmy_before, $S/fix5/simmy_after.
Outputs: /mnt/user-data/outputs/fix_social_free_play.png (before/after, 1080p offscreen),
/mnt/user-data/outputs/fix_social_split_2p.png (two players, wide split, player 2's social menu).

## The report and the user's log
One player, free play, lot 13. Their stdout shows: `#79 queues "Socialize" on #81 (menu)`. #81 (the
woman) was busy talking with #80. #79 then runs Socialize on social_interactions_socialize #186 and
waits for her. `player 1 cancels: 1` (B). #81 finishes her talk, runs "Socializer", both are done,
and #186 is killed. After that there is only `player 1 cancels: 0` and no queue line, so A did nothing.
There is no `[social] ... starts social mode` line: the social menu never came up.

## Root cause
Tree trace (SIMMY_VM_TRACE=79, social_interactions_socialize 1011 "Social Mode Core_Person A"):
- Node 92: `mydata[0xb] == global[3]` (the sim is the selected one). Node 90 then runs TrySocialMode
  **step 4** on the player's sim *before* the partner arrives. Step 4 sets person data 0x28 =
  partner and SetFirstPlayerInSocialMode(true). This matches GC 801282E0.
- Next comes 44 "SG_common Person A entry". If the Socialize is cancelled (B) or the partner never
  comes, the entry fails. The tree then takes 79 -> 43 "SG_exit person A" -> 20d7 (it waits up to 450
  ticks for the partner) and returns **without step 1**, so social mode is never ended.
- The sim is left with the first-player flag and 0x28 set. DirectInteractor::OnCommandPressed
  (the GC gate from q1 F8) ignores A, Z and next/previous sim while IsInSocialMode. B still cancels,
  but the queue is empty, so it prints `cancels: 0`. That is the soft-lock.
- GC recovers in **DirectInteractor::Update (80204838)**. At the top it runs
  `if IsWaitingForSocialMode (0x35C) -> CheckSecondPlayerForFailedSocialModeEntry (0x37C)` and
  `if IsFirstPlayerInSocialMode (0x34C) -> CheckFirstPlayerForFailedSocialModeEntry (0x374)`.
  CheckFirst (8012B8CC) runs when the person runs no interaction (GetCurrentInteractionStackObject,
  vtable 0xF4, is 0). It clears the first flag and person data 0x28, and the animator goes to
  StartDefaultIdle. In the port, DirectInteractor::Update skipped these checks ("GC: also social-mode
  entry checks"), and both Check* functions and GetCurrentInteractionStackObject were generated stubs.
  So nothing ever cleared the state.
- Vtable mapping: TrySocialMode uses 0x33C SetFirst, 0x344 SetSecond and 0x364 SetWaiting.
  CheckNewSimInSocialMode uses 0x34C IsFirst, and SimInteractor already used 0x35C -> 0x37C for
  IsWaiting -> CheckSecond. The order is IsInSocialMode 0x334, SetFirst 0x33C, SetSecond 0x344,
  IsFirst 0x34C, IsSecond 0x354, IsWaiting 0x35C, SetWaiting 0x364, GetSocialModeTarget 0x36C,
  CheckFirst 0x374, CheckSecond 0x37C. SAnimator2 0x13C is StartDefaultIdle (tools/vslot.py).

## Fix (faithful)
- socialmode.cpp: ported cXPersonImpl::CheckFirstPlayerForFailedSocialModeEntry (8012B8CC),
  CheckSecondPlayerForFailedSocialModeEntry (8012B978: two players only, DAT_80475C70 = _gd.m_fieldEC;
  clears waiting, second, 0x28 and idles both; also cancels the other player's sim's actions and clears
  its first flag and 0x28), and GetCurrentInteractionStackObject (8012640C: the queue head's stack
  object while m_hasCurrentAction).
- directinteractor.cpp: DirectInteractor::Update now starts with GC's two entry checks.
  DirectInteractor::OnStart now does GC's social part (SetFirstPlayerInSocialMode(false), person
  data 0x28 = 0), as SimInteractor::OnStart already did. The input session and plumb bob part stay a stub.
- SimInteractor::Update already called CheckSecond (it was a stub, now real). GC's classic Update has
  no CheckFirst check, and that was left as GC.

## Verification (headless + offscreen, pad only)
- Repro, free play lot 13 by pad (SIMMY_PAD_SEQ "start,right,a,right,a,up,a,a,a,a,house,a,a", then
  A, A (person list), A (Socialize) on a busy Freeber, then B, then A presses), SIMMY_SIM_START=19,13:
  - before (simmy_before; q3/q4/r2_before/oc_before/hd_before): the same log pattern as the user's
    (Socialize, cancels 1, Socializer, socialize killed, cancels 0). Later A presses: no menu, no queue.
  - after (r2_after/oc_after/hd_after): after the socialize object goes, A opens the action menu
    again. Socialize on the woman queues, social mode starts, the social menu shows (8 socials), and B
    backs out ("chooses -2", "leaves social mode"). hd_after also covered the partner-never-comes case
    (#81 went to eat, Socializer never ran): it recovered the same way.
- Each Freeber (#80/#81/#82; fs80/81/82, Socialize via SIMMY_INTERACT_NEAR=#id, SIMMY_SOCIAL_PICK
  "@Talk,Tell Joke", 2 rounds): social mode starts, 2 socials, menu back-out, leaves social mode, and
  then Z gives "classic control", which shows the gate is open again. Relationships change ([rel]).
- Story lot 9 housemate (#98; l9_before/after, Socialize then B at once): before, Z was ignored
  afterwards (stuck). After, "classic control".
- Paperboy (npc_paperdelivery #185 on lot 9, pb_*): Socialize is not offered while he delivers
  (1 object near, nothing queued), before = after, no lock.
- Classic cursor (Z): the person's classic menu offered a social directly ("Tell Joke" on #82, cl3), with
  no Socialize and no social mode. The menu worked afterwards. GC's SimInteractor::Update has no
  CheckFirst, so a failed Socialize entry from classic would not be cleared on GC either. The classic
  menu did not offer Socialize, so this was not reached.
- Two players (tp1: player 1 -> player 2's sim; tp2: player 2 -> player 1's sim; SIMMY_TWO_PLAYER=1,
  p2:none): social mode, 2 socials, back-out, then both players' Z works. Split wide offscreen (splitw):
  both views, player 2's social menu in the right half.
- B-cancel suite (suite/, SIMMY_FUNDS=1000 SIMMY_STORY=0): bladder 56.9, comfort 28.3, hygiene 45.6,
  bed energy 31.8, fridge hunger 32.0, "cancels: 1"; 0 TREEASSERT / keeps failing. This matches fix4
  (57.0/28.3/45.6/31.8/1).
- Story start (lot 9, 7000 frames): before = after (3 dialogs, 0 TREEASSERT, 0 keeps failing).

## Known / not changed (follow-ups)
- The port's IsInSocialMode() is first||second flag. GC 8012CA2C is `person data 0x28 != 0`, and
  GetSocialModeTarget (8012BB20) is the person named by 0x28 (the port keeps its own map). They agree
  for the selected sim in every path tested. They differ for the partner (0x28 set, no flags): head look,
  sprites and the F8 gate on the partner player's sim in 2P. This was left alone (many users: esim,
  sanimator2, sprites, boneparticle, acttarget).
- cXPersonImpl::Cleanup (8012A04C) has a GC social part: an involved selected sim leaves social mode
  and the player goes back to interactor 0. It is still not ported. No run here hit an involved
  cleanup ("lets go of" never logged).
- After B, the cancelled Socialize can stay in the queue while the partner is still busy. The data tree
  20d7 "SG_wait for Person B state on exit" waits up to 450 ticks. This is GC data, unchanged.
- In classic mode the person's menu offers individual socials. This was not compared with GC here.

## Files vs base69 (2 files, 4 hunks; all in session_full.diff)
- src/game/socialmode.cpp (port-owned social file): 2 hunks (CheckFirst/CheckSecond after
  SetWaitingForSocialMode; GetCurrentInteractionStackObject before TryTestInteractingWith).
- src/game/directinteractor.cpp: 2 hunks (OnStart: social part of 80203E6C instead of the stub text;
  Update: the two GC entry checks at the top + comment).
MSVC: no new headers, no VLAs or extensions, no reference params on operators, no using-declarations.
Scripts: hs.sh (headless free play -> lot 13 by pad sequence; PRE= for another front-end path),
os.sh (offscreen, same; HD=0 low res), hd.sh, suite/.
