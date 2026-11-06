# fix6: Socialize: busy partner never comes or quits at once, and no hourglass (lot 13, 1P, pad)

Tree /home/claude/gfx/port (base70 copy). Build /home/claude/gfx_build (Ninja, clang, RelWithDebInfo).
It is configured like simmy_build: SIMMY_AURORA_DIR=/home/claude/tools/aurora, FETCHCONTENT_SOURCE_DIR_*
-> tools/deps and simmy_build/_deps/*-src, the SDL_* OFF flags, SDL_UNIX_CONSOLE_BUILD=ON,
AURORA_ENABLE_TESTS=OFF. Full diff vs base70 (gen/ and __pycache__ excluded): $S/fix6/session_full.diff.
Outputs: /mnt/user-data/outputs/fix6_social.png (social menu and interest meter, 1080p offscreen) and
/mnt/user-data/outputs/fix6_hourglass.png (waiting, hourglass in the bottom right).

## Reading the user's log
- Try 1, game_out 95-114: #81 had just picked "Cook Generic Meal" (priority 2) and was walking to the
  fridge when #79 queued Socialize. The Socializer (pushed at 100 by person SG 208e "Push Social" /
  2111 "SG_Should Push Max Priority") had to wait. In the port a walk cannot be interrupted, so she
  walked on, opened the fridge, and only then ran Socializer, about 500+ frames later. Nothing on screen
  showed that the player's sim was waiting, so the player pressed B twice ("cancels: 1"). That cancelled
  the Socialize. Its exit (2001 / 20d7 "wait for Person B state on exit") waits for B. When B arrived she
  saw the exit state, so both were "done" and #182 was killed. The "joins then quits at once" step is the
  normal result of that cancel.
- Try 2, 143-147: #81 was running "Eat" at priority **100**. food_counter_sg 2006 "Prepare Core SG"
  node 32 pushes Eat with `push 0203010f`, priority mode 1 (100), as a continuation. That is GC data
  (checked with a temporary push trace, since removed). A Socializer at 100 does not pre-empt a
  priority-100 action (ShouldInterrupt needs a strictly higher one; it matches GC 801264C8). So she eats
  first, and the player's sim waits with nothing on screen. Without B (eat100 run), A waits 450 loops,
  walks to where B was, and waits in 2077. Then it gives up and B quits at once on arrival. This is the
  same log pattern as the user's, with no B pressed. GC does the same with this data; the difference is
  that GC shows the hourglass for the whole wait.

## Root causes
1. **No hourglass (CSMTarget::Update, 801A6E00).** The CXM widget's hourglass (CXM_hourglass_swf,
   s2c_comp_hourglass.swf at 550,370: bottom right) shows on GC while the CXM list is closed and the
   selected person has `GetIdleState() == 3` (vtable 0x16C, person data 0) or `SAnimator2 +0x10 == 1`
   (routing). The social tree 1011 "Social Mode Core_Person A" node 78 sets `mypd[0] := 3` before
   "SG_common Person A entry" and clears it at nodes 79 and 80. In the port, `m_hourglassWanted` was
   hard-coded to 0 ("PC: neither is tracked"), so it never showed.
   - The ACT action queue (ACTTarget) is *not* the bottom-right widget in direct control. GC's
     ACTTarget::Update hides it while `GetPlayerInteractor(player, 0)` (the DirectInteractor) is active,
     or while the sim is in social mode. It shows only in classic (Z) control. The port already did
     this, and it is unchanged.
2. **A walking sim could not be interrupted (cXPersonImpl::TryGotoRoutingSlot, 80120640).** GC's route
   state machine calls ShouldInterrupt (person vtable 0x2E4) and SAnimator2::IsInterruptable (0x8C,
   8005EDB4, which always returns 1). It does so in state 1 (before it sets out) and in states 2 and 3
   (while following). If both are true, the route ends: route failure 4 -> state 0xB (EndFollow) -> 7,
   which returns false. The tree then ends the action, and the queued higher-priority one (the
   Socializer) starts. The port's walk wait was just `if (result == 0) return 2;`. A busy partner
   walking to something (cooking, the toilet, putting food away) kept going and finished the next step
   first. B on the player's own walking sim did not stop the walk either.

## Fix (faithful)
- src/game/csmtarget.cpp (CSMTarget::Update): `m_hourglassWanted` follows GC: CXM list not shown,
  selected person, `GetIdleState() == 3 || GetSAnimator()->m_routing == 1`. With SIMMY_SIM_TRACE there is
  a dev trace "[hud] player N: hourglass on/off".
- src/game/person.cpp (TryGotoRoutingSlot):
  - Start (GC state 1): if `ShouldInterrupt() && IsInterruptable()`, failure 4 and the route returns
    false without setting out.
  - While walking (GC states 2 and 3): the same test leads to `AbortRoute()` (the port's EndFollow
    equivalent: the sim stops where it stands), failure 4, and false. Route traces print "interrupted
    before it starts" and "walk interrupted".
- src/game/sanimator2.cpp: SAnimator2::IsInterruptable (8005EDB4) is ported (`return 1`). It was a
  generated stub that returned 0.
MSVC: one standard header (<cstdlib>), no VLAs or extensions, no operator/reference/using changes.

## Verification (headless unless noted; scripts $S/fix6/hs.sh, os.sh, hd.sh; logs $S/fix6/logs)
| run | case | result |
|---|---|---|
| busy81, busy81b, busy81c (before) | PICK Socialize #81, idle / at fridge | social mode works, but the busy partner arrives ~500-800 frames later with no feedback |
| m3 (hourglass only) / m3b (both fixes) | real pad menu (A, A "Socialize") on #80 walking to the toilet | m3: hourglass on, #80 still finishes the toilet first (~900 frames); m3b: "#80: walk interrupted", Socializer at once, social mode ~700 frames sooner, hourglass on->off, 2 Talks, leaves |
| m3c | menu Socialize, then B while waiting | "cancels: 1", hourglass off, both done, object killed; A later opens the menu again (control back) |
| m3e | B during the stance change before the menu, then B on the menu | H2D B cancels the Socialize (GC H2DTarget::Update path); menu shown; B on the menu: "chooses -2", leaves social mode; control back |
| f80 / f81 / f82 / eat81 | PICK Socialize on each Freeber, busy | #80 "interrupted before it starts" (toilet); #81 interrupted mid-walk and her clean-up routes fail (code 4); all start social mode, Talk x2, leave |
| eat100 | #81 eating at priority 100 (user's try 2) | hourglass for the whole wait; A walks to her after the 450-loop wait; she keeps eating (GC data), so A gives up; B then quits at once. Matches GC data; the hourglass is the feedback |
| ob1 (offscreen 1080p, pad menu) | Socialize #81 (busy, Eat 100 continuation) | hourglass visible (fix6_hourglass.png); social menu and interest meter (fix6_social.png); Tell Joke chosen by A, both animate (ob1_3850); the social menu later timed out and left social mode |
| tp1 (2P, story lot) | P1 Socialize on P2's sim | social mode, Talk; P2's hourglass shows while their sim walks over |
| cl (classic, Z) | Z, cursor, A A A "Tell Joke" on #80 | queued, Tell Joke / Listen to Joke run; hourglass while #79 walks |
| suite ($S/fix6/suite, copy of m16/suite.sh run on this build, own XDG dirs) | toilet 57.4, sofa 28.3, fridge 31.7, shower 46.0, bed 31.8; cancel "cancels: 1" = 1 | 0 TREEASSERT, script errors or "keeps failing". Within run-to-run jitter of m16/fix5 (56.9-57.2 / 28.3 / 32.0 / 45.6-45.8 / 31.8) |
| story (default seq, 7000 frames) | lot 9 story start | 3 dialogs, 0 TREEASSERT, 0 keeps failing, no interruptions logged |

## Left as is / notes
- A busy partner whose action runs at priority 100 (Eat after counter prep, a GC data continuation) does
  not come until it ends, and the Socialize can time out. This is GC data and logic. The hourglass now
  shows the wait. GC's route state machine also re-plans toward a moving target and plays
  "sg_route waiting animation" / "route failure feedback". The port has one route try with no failure
  trees (unchanged).
- H2D B during the social stance change (before the menu shows) cancels the Socialize, through GC's
  H2DTarget::Update path. The menu still comes up, and B or the menu's timeout ends it. Unchanged.
- IsInSocialMode first||second vs GC `person data 0x28 != 0` (fix5 note): unchanged.
- CSMTarget's "context" part (the carried object's name and "csm_more"; vtables 0xAC/0x174/0x17C):
  still not ported.
