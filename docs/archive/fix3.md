# fix3: windows drawn as a flat pale outline from inside (tree /home/claude/gfx/port, merge base /home/claude/base67/port)

Full diff (gen/ excluded): $S/fix3/session_full.diff. Before binary: $S/fix3/simmy_before.
Output: /mnt/user-data/outputs/fix_window_inside.png (before | after: lot 13 user view 1080p day, night, lot 13
front walls-up outside, lot 2 front).

## Symptom
Freebers' house (lot 13), bookshelf room: the window right of the wall lamp showed as a faint outline on a solid
wall (no hole, no glass). Reproduced with lot13_off.txt + SIMMY_DIALOG_AUTO=30 SIMMY_SIM_START=22,19, pad
"2600:cdown/30" (l13h_2750.png = the user's view). It was not only inside: on lot 13 the two front windows
(16,27 / 23,27, behind the topiaries) had no hole from outside either.

## Root cause
EIWallPart::GetWallModelId (8002BD58) picks the wall model with the window/door cut-out from the portal it finds
with ObjectIterator over the wall part's tile. GC: a tile holds one object in the module's tile map; the other
objects on the same tile hang off it in the slot hierarchy: the object's slot vector (+0x90, read from the save by
ReconSlots) entry 0 is the tile's object chain (the next object stacked on the tile; its data 0x1A = parent,
data 0xE = 0), entries 1.. are the object slots (GetObjectSlot(i) 800DD578 = entry i+1). ObjectIterator::operator++
(800F2AC0) walks that vector (HierGetChild) and climbs with HierarchySite (data 0x1A / 0xE).
The port's operator++ only walked the object slots emulated with data 2/3 (GetContainedObject) and never the
saved slot vector (cXObjectImpl::m_slotContents), so a window part that shares its tile with another object (a
wall lamp, plant, shrub; e.g. tile 19,11: owner #13 slots [41,0,0], window #41 data 0x1A=13) was invisible to the
wall: plain wall model on that side, the window model hidden in the wall (its flush inner frame z-fighting =
the "outline"). Not related to fix1 (SetObjOrient/IsDynamic): window placement is unchanged.

## Fix (faithful to 800F2AC0)
- objectmodule.cpp ObjectIterator::operator++: entry 0 = the tile chain (m_slotContents[0], accepted while that
  object still names this one in data 0x1A, is held by nothing (data 2 == 0), stands on the same tile and is not
  the root - PC guard against stale saved links, since the port's PlaceObject keeps containment in data 2/3);
  entries 1.. = the port's object slots (GetContainedObject(entry-1)); going up: a data-2 container (next entry =
  data 3 + 2) else the chain parent (data 0x1A, next entry 1).
- eroom.cpp GetWallModelId, style 0x16: without a match the GC uses the LAST OBJECT VISITED (a portal: its cut
  row; otherwise the plain wall), not the last portal; matters now that the iterator visits more than one object.
  (Verified: no wall model changed by this part on lots 13/9/2.)

## Verification
- Temporary trace of every portal-style wall part (tile, segment, style, model), old vs new iterator:
  lot 13: 10 wall sides changed plain -> cut (11,10 seg1+2, 19,10/19,11, 21,10/21,11, 11,25, 11,9, 16,27, 23,27);
  lot 9: 1 (12,19 seg 1); lot 2: 4 (7,7 / 11,7 / 21,7 / 25,7 seg 8). Everything else identical.
- Visual (l13run.sh / storyrun.sh, four facings each): lot 13 cut-away day before/after (c1*), walls up outside
  before/after (c0*), walls down (c2a), night cut-away before/after (n1*), night walls up (n0a); lot 9 cut-away day
  before/after (s9*), night walls up (n9a); lot 2 day before/after (l2*), night walls down (n2a). Doors, paintings,
  wall lamps unchanged; windows now show glass and the outside through them, from inside and outside.
- Note: on lot 2 a showering sim's censor mosaic now shows under a front window (the room behind is visible through
  the window portal; the censor is drawn last over walls, as for any visible room) - expected.
- B-cancel suite (suite/, SIMMY_FUNDS=1000 SIMMY_STORY=0): bladder 56.9, comfort 28.3, hygiene 46.3, bed sleeping
  energy 31.8, "cancels: 1" - as before. Story start lot 9 (story_300.png), split wide (splitw_400.png): fine, no crash.

## Not changed (noted)
- The port's PlaceObject/SetLocation keep the tile map "first object wins" and never write the slot vector / data
  0x1A / 0xE (GC HierSetSite overwrites the tile map and links the hierarchy). Objects placed during play on an
  occupied tile are still not in the tile chain; a window bought onto a tile with a lamp would still miss its cut.
- GC's non-0x16 branch also does TileWalls::SetStyle(seg, 1) when the tile's last object is not a portal; not ported.

## Files vs base67 (4 hunks, all in session_full.diff)
- src/game/objectmodule.cpp: 2 hunks (comment + TileChainChild helper; operator++ body)
- src/game/eroom.cpp: 2 hunks (style 0x16: track the last visited object; the fallback line)
MSVC: no VLAs/extensions, anonymous-namespace helper, no new headers.
Scripts: l13run.sh, storyrun.sh, sheet.py, hd.sh/shot.sh (fix1's), suite/.
