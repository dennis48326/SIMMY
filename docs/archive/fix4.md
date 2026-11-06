# fix4: two invisible things with blob shadows you bump into (lot 13, Freebers)

Tree /home/claude/gfx/port, build /home/claude/gfx/build, merge base /home/claude/base68/port (untouched).
Full diff (gen/ excluded): $S/fix4/session_full.diff. Output: /mnt/user-data/outputs/fix_invisible_people.png.

## What they are (not people)
They were not people. The Freebers are read once from the house's save (4 type-2 objects, ids 79-82, no
"made" lines). Over 40 traced loads (free play, Continue, evict/return, move, dev menu re-entries, 2P,
offscreen and headless, MALLOC_PERTURB_, x5 speed) every person had one ESim, model ready, parts present,
and no person's body stayed undrawn while its shadow was drawn.
They are **metal-detector seeds**: `electronics_metal_detector_seed`. These are hidden (data 0x22 = 1),
zero-extent (data 8 = 4) objects. On every house load, the lot's `electronics_metal_detector_seeder`
(lot 13: #77) runs "Generate Seeds" (200d): it creates 5 seeds, puts each on a random tile
(TryFindGoodLocation mode 6), then keeps the seed only if `room[1] != 0` (the seed's room is outside).
Otherwise it retries up to 5 times and then kills the seed.

## Root cause (three PC defects)
1. **Room scope stub.** InterpValue scope 0x17 (8010AAC8) was a stub that returned 1 for data 1 for every
   room, so every room looked like it was outside. The seeder kept seeds wherever they landed, including
   inside the house.
2. **Collision.** CollisionMap::DoesCollideObject used only flag 0x10 for CanIntersectPeople.
   GC's 800EB6C4 lets people pass through a zero-extent object (flag 4) unless its flag 2 is set. Seeds
   have flags = 4, so on PC they blocked the sim in direct control, both indoors and outdoors.
3. **Visible blobs.** An indoor seed has a model (type 4), so its instance is in the room's shadow list.
   ERLevel::DrawObjectShadowsInside (same as GC) then draws the soft shadow square under it, while the
   seed itself is hidden. That gives a blob with nothing above it.

**Why it was intermittent:** the seeds are regenerated on every load (load tree 2009 sets attr 1, and
main 200b then removes all seeds and regenerates them). The random tile came from `std::rand()` (a PC
stand-in), whose sequence depends on how many std::rand calls happened earlier in the process: autonomy,
routing and CAS also use std::rand, and those depend on frame timing. MSVC's rand also differs from
glibc's. Some loads put 0 seeds indoors, some put 1 or 2 near the sims. With glibc on this machine, free
play lot 13 placed all 5 outside, but a dev-menu entry (sk3) put one seed in room 2. GC uses
GetNextRandomNumber, which is the simulator's sequence during the tick.

## Fix (faithful)
- objectsim.cpp InterpValue case 0x17 is now ported from 8010AAC8. The room id comes from temp 0;
  0xFFFB (-5) gives 0 / 1 / 0 / 0 for data 0..3. Otherwise RoomManager::GetRoom is used: data 0 is
  GetAmbientLight*100, 1 is IsOutside, 2 is 1, 3 is GetArea, 4 is the outdoor score if the room is
  outside, else the room's +0x7C. A missing room gives error 0x28, and data above 4 gives error 0x29.
  GetAmbientLight and the room stats are still the existing stubs/zeros, so those values are unchanged.
- objectsim.cpp TryFindGoodLocation mode 6 now uses GetNextRandomNumber (GC 800FADF4: x first, then y)
  instead of std::rand.
- collisionmap.cpp DoesCollideObject now uses the full CanIntersectPeople rule from 800EB6C4.
- livemode.cpp BGExec_LoadHouse now computes the rooms (world ComputeRooms + object room ids) before the
  objects' PostLoad. GC order: HouseImpl::LoadFile ends with RoomManager::UpdateRooms, then
  NeighborhoodImpl::LoadHouse calls ObjectModule PostLoad (vtable 0x34). Without this, the real room
  scope gave 11 TREEASSERT 0x28 at load: the lamps' "SG_AutoLights" load trees (202f node 9) asked about
  window rooms before any room existed. The existing call after MoveFamilyOntoLot stays (RefreshHouse).

## Verification
- Seeder trace (SIMMY_VM_TRACE=77 / TRACE_TREES=200d): after the fix, indoor placements are rejected
  (200d:11 -> 0) and retried. sk4 (dev menu, 2 entries): 4 rejected, 10 kept, all in room 0.
- Test-only hook (not shipped; binaries $S/fix4/simmy_{before,after}_hook, SIMMY_TEST_SEED_AT=x,y;..)
  forced seeds at tiles 21,14 and 20,12 next to Winston, plus a collision probe that walks a 0.24 circle
  into each seed. Before: seeds kept in room 4, every seed BLOCKED (also outdoor ones). After: seeds
  rejected indoors and placed outside, all "passes". Offscreen 1080p before/after: the diff shows the two
  soft squares by Winston (output png). This llvmpipe renderer draws the indoor blob shadows faintly, so
  the panel uses an x6 diff.
- No TREEASSERT, "keeps failing" or crash: free play lot 13 (fx2), story start lot 9 (7000 frames x5,
  before = after: 0/0, 16 dialogs), dev lots 2/9 soaks (0 before and after), Create-a-Family move-in
  (house 15), free-play save + Continue (house 13, the 4 Freebers read, 185 objects), two-player split
  wide (both sims, splitw.png).
- B-cancel suite (suite/, SIMMY_FUNDS=1000 SIMMY_STORY=0): bladder 57.0, comfort 28.3, hygiene 45.6,
  bed energy 31.8, "cancels: 1". Same as before (fix3 56.9/28.3/46.3/31.8/1, fix2 hygiene 45.6).

## Known / not changed
- Existing PC saves: seeds already saved indoors stay until the seeder reseeds. On Continue the seeder
  resumes its idle (0x4E20 ticks) first. They no longer block the sim, but their indoor blob shows until
  the reseed.
- "Sub - Seed - Spawn New" (2028, after digging with a detector) uses goodloc mode 6 without a room check,
  as on GC, so GC could also put a spawned seed indoors. Left as GC.
- Other std::rand stand-ins (autonomy, routing scoring, CAS, wants) were not touched.
- Ruled out along the way: people double-made, ESim/shadow leaks across re-entry (the level's listed
  shadows start at 0 each load), BG-thread ESim creation (initModel runs on the app thread), the
  near-camera fade (alpha recovers), and fix3's tile-chain iterator (it never visits people twice).

## Files vs base68 (3 files, 4 hunks; all in session_full.diff)
- src/game/objectsim.cpp: 2 hunks (InterpValue case 0x17; TryFindGoodLocation mode 6 RNG)
- src/game/collisionmap.cpp: 1 hunk (DoesCollideObject CanIntersectPeople)
- src/game/livemode.cpp: 1 hunk (+4 lines: rooms before PostLoad)
MSVC: no new headers, no VLAs or extensions, no reference params on operators.
Scripts: hl.sh (headless), hd.sh / sheet.py (offscreen), soak13.sh (dev-menu re-entry soak), batch1.sh,
hook.py (the test hook, applied only to the *_hook binaries), suite/.
