# fix1: menu default selection + floating windows/doors (tree /home/claude/gfx/port, merge base /home/claude/base63/port)

Full diff (gen/ excluded): $S/fix1/session_full.diff. Before binary: $S/fix1/simmy_before.
Scripts: shot.sh / hd.sh (g9's, writing into fix1), cmp.sh NAME LAST SHOTS PADSCRIPT (before + after
binary, frame script, NAME_cmp.png rows before/after), cmpseq.sh (same with SIMMY_PAD_SEQ, frames from
house), lot13_off.txt (offscreen frame script into 8 Rockpile Rd / lot 13, house up by ~2000),
py/unbig.py (unpacks a FLSH resource's BIG: resdump.py save flashes <movie> x.flsh; unbig.py x.flsh dir;
then $S/apt2/dis.py dir/<movie> out.dis disassembles its ActionScript), suite/ (B-cancel suite, this build).

## Bug 1: no item highlighted when a menu first appears (Apt runtime, general)
Seen in free play's Places to Go (s2c_main_families, lot_select clip = char 70) and the lot menu
(occupied/unoccupied). The movie selects the first item itself: lot_select's frame 2 does
mySpatialList.addOption(folder_N) x4; addOption calls updateLinks(undefined), which calls
options[0].select() -> folder_0.gotoAndPlay("selected") (frame 3, the first frame of the green glow
fade-in, alpha 0). The folder clips (char 47) have their own frame-2 action stop().
Order in a tick: char 70 advances to frame 2 and queues its action, then its children advance to their
frame 2 and queue stop(). The queue runs: char 70's script jumps folder_0 to frame 3, then folder_0's
queued frame-2 stop() ran -> folder_0 stuck on frame 3 (glow alpha 0): nothing visible until the
player moved.
GC (AptAnimationPoolData::runActions 80283FA0, AptActionQueueC::AddActionBack 80284FD8, AptCIH::tick
80293128, jumpToFrame 80292994): each queue entry stores the clip's +0x28 frame marker; tick sets it to
-frame around queueFrameActions, jumpToFrame to +frame; runActions skips an entry with a negative marker
when the clip's current frame is no longer -marker. So the stale stop() is dropped and the glow plays.
Fix (libaptz, runtime-wide): AptQueuedAction::frameMarker, addAction/queueFrameActions take the marker
(tick: -next, jumpToFrame: +frame, others 0), runActions skips stale timeline actions.
Checked before/after: Places to Go (fixed), occupied/unoccupied lot menus (fixed: Enter House / Move in a
Family green), family select (already OK: hilite_box), main menu (OK both), pause root, Options submenu
(Save green in both), wants & fears, buy catalogue, story CAS: unchanged. Side effect: the FAM screen's
load-in is no longer delayed by a few frames.
Output: /mnt/user-data/outputs/fix_menu_default_focus.png.

## Bug 2: windows (and doors) half a tile into the room
Doors and windows are 2-part multi-tile portals (one part each side of the wall, only one part has a
model). Their models (window_punched_*, door_*) are centred on their origin (y +-410 units = the wall's
thickness; wall-part models have the wall at y = -4096 = -0.5 tile), so the origin must be on the wall line.
ISimsMultiTileObjectModel::SetObjOrient (80051B94) and ISimsWallObjectModel::SetObjOrient (8005127C)
choose between the group's tile-rectangle centre and the part's own tile with a call through the part's
**cXMTObject** interface, vtable +0x44 = cXMTObjectImpl::IsDynamic (800C71F0: GetTreeID(8) != 0). The
port had read it as cXObject's +0x44 (IsPortal), so every door and window used its own tile centre:
0.5 tile off the wall into the room (the hole stayed in the wall part). Fix: ported
cXMTObjectImpl::IsDynamic (mtobject.cpp) and use it in both SetObjOrients (isimsobjectmodel.cpp).
Not related to the sims' removed +0.5 house offset (objects keep tile + 0.5) nor to the session-27 slot
offsets (portals are in no container).
Verified (temporary trace, removed): every door and window on lot 13 (facings 0/2/4), lot 9 (story,
0/2/4/6) and lot 2 (0/2/4/6) now has one world coordinate on an integer wall line and the other at .5;
before all were at .5/.5. Visual: window 59 (24,18 dir 2, the user's), door 31 (15,21 dir 2), window 41
(19,11 dir 0). Output: /mnt/user-data/outputs/fix_window_in_wall.png.

## Files vs base63 (all hunks in session_full.diff)
- include/simmy/classes/aptplayer.hpp: 3 hunks (queueFrameActions marker param, AptQueuedAction::frameMarker, addAction param)
- src/libaptz/aptplayer.cpp: 5 hunks (queueFrameActions, jumpToFrame, tick, addAction, runActions)
- src/game/mtobject.cpp: 1 hunk (IsDynamic 800C71F0)
- src/game/isimsobjectmodel.cpp: 3 hunks (IsDynamicMultiTile helper, wall-object and multi-tile SetObjOrient)
MSVC: default arguments, dynamic_cast (RTTI already used), no VLAs/extensions.

## Noted, not fixed
- Offscreen pad-seq "start,a,right,a,up,a,a,left,a,house" without SIMMY_CAS_ACCEPT crashes in story CAS
  after pressing A on genetics (both before and after binaries; pre-existing).

## Verification (final binary)
- B-cancel suite (suite/, SIMMY_FUNDS=1000 SIMMY_STORY=0): toilet bladder 57.9, sofa comfort 28.3, shower
  hygiene 46.4, bed energy 32.7 (sleeping), cancel "cancels: 1" - same as earlier sessions.
- Story start (SIMMY_CAS_ACCEPT=300): lot 9, live mode (story_300.png). Split wide (SIMMY_TWO_PLAYER=1
  --split=wide): both views fine (splitw_400.png; story_split.png has both). No crash in any run.
