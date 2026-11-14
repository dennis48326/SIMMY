@echo off
rem SIMMY PC objects (PC mod: mods): convert objects, wallpapers and floors from YOUR copy of The Sims 2
rem (PC) - the base game and every expansion and stuff pack installed under it - into the mods folder
rem next to simmy.exe.
rem Needs Python 3 (python.org; tick "Add to PATH") - the first run installs numpy and Pillow for it.
rem Nothing is downloaded from EA or anywhere else; see docs\mods.md.
rem
rem   convert_pc_object.bat                     every buy-mode object the converter knows - Decorative,
rem                                             Surfaces, Seating and Beds, Appliances, Lighting, Plumbing,
rem                                             Electronics, Hobbies and Miscellaneous (Storage, Kids and the
rem                                             rest) - and every wall and floor covering of every pack
rem                                             found (default: the complete set)
rem   convert_pc_object.bat decorative          only the Decorative objects
rem   convert_pc_object.bat surfaces            only the Surfaces objects (counters, tables, desks, ...)
rem   convert_pc_object.bat seating             only the Seating objects and Beds (chairs, sofas, stools, beds, ...)
rem   convert_pc_object.bat appliances          only the Appliances (fridges, stoves, microwaves, dishwashers, ...)
rem   convert_pc_object.bat lighting            only the Lighting (table, floor, wall and ceiling lamps, outdoor lights)
rem   convert_pc_object.bat plumbing            only the Plumbing (toilets, showers, bathtubs, sinks, hot tubs)
rem   convert_pc_object.bat electronics         only the Electronics (TVs, computers, stereos, phones, alarms,
rem                                             arcade and pinball machines, vending machines, ...)
rem   convert_pc_object.bat hobbies             only the Hobbies (bookcases, easels, instruments, telescopes,
rem                                             chess and pool tables, exercise machines, workbenches, ...)
rem   convert_pc_object.bat storage             only the Storage (dressers, armoires, wardrobes, lockers,
rem                                             closets, the coat rack)
rem   convert_pc_object.bat kids                only the Kids (toy boxes, teddy bears, the dollhouse, toddler
rem                                             toys, ... and the remote-control toys)
rem   convert_pc_object.bat misc                only the rest of Miscellaneous (bars, the juice keg, the
rem                                             bubble blower, bonfires, trash cans, clothing booths, buffets,
rem                                             the sprinkler, cakes, balloons, arches, the windmill, pet beds
rem                                             and houses)
rem   convert_pc_object.bat walls               only the wall coverings for build mode (paint, wallpaper,
rem                                             paneling, tile, stone, brick, siding, ...: about 1480)
rem   convert_pc_object.bat floors              only the floor coverings for build mode (carpet, wood,
rem                                             tile, linoleum, stone, brick, concrete, ...: about 650)
rem   convert_pc_object.bat decorative seating  several categories (walls floors: both coverings)
rem   convert_pc_object.bat list ficus          list PC objects whose name contains "ficus"
rem   convert_pc_object.bat inventory           write the inventory (mods\pcobj_inventory.tsv) only
rem   convert_pc_object.bat "No-Fuss Ficus"     one object (name or GUID); the GameCube base is chosen
rem   convert_pc_object.bat "No-Fuss Ficus" plant_floor_rubbertree     ... with this GameCube base object
rem
rem Each object behaves like its GameCube base (decorative: chosen by where it goes: wall, floor,
rem table top, ceiling, rug; surfaces: a GameCube counter or table of the same footprint, with things
rem put on it at the PC top's height, counters joining up in rows and corners; seating and beds: a
rem GameCube chair, sofa, bench or bed of the same footprint and comfort, sims sitting and lying at the
rem PC seat's height; appliances: the GameCube fridge, stove, microwave, dishwasher, espresso machine,
rem food processor, blender or trash can that does the same, with the PC doors opening as the
rem GameCube's do; lighting: the GameCube's table, floor, wall lamp or street lamp, lighting the room in the
rem PC lamp's colour, its shade glowing when on, switching on at night by itself as the GameCube's lamps
rem do; ceiling lamps hang from the ceiling, inside only, sims and furniture under them; plumbing: the
rem GameCube's toilet, shower, bathtub, sink or hot tub of that kind and price, sims sitting, standing and
rem lying in the PC model, with the PC's dirty and clogged looks; shower / tub combinations are bathtubs,
rem counter sinks go into counters, also converted ones, which show a hole for them; electronics: the
rem GameCube's TV, computer, stereo, jukebox, DJ booth, arcade or pinball machine, game console, phone,
rem alarm or vending machine that does the same, the PC TVs and computers showing the GameCube's screens:
rem channels, games, job search; hobbies: the GameCube's skill object teaching the same skill - bookshelf,
rem easel, piano, guitar, chess table, telescope, weight bench, hamster wheel, punching bag, invention
rem bench - with the GameCube's canvas, guitar, barbell and rocket in the sims' hands, and the pool tables
rem as the GameCube's air hockey table on its Misc tab; storage: the GameCube's dresser (two tiles wide)
rem or wardrobe (one tile) on its Misc tab, sims changing clothes and planning outfits at the PC one;
rem kids: the remote-control toys as the GameCube's game console, the toys and toy boxes as decorations,
rem cribs, high chairs, potty chairs and changing tables left out - the GameCube has no babies or
rem toddlers; misc: bars as the GameCube's drink bar, the juice keg and nectar bar as its keg, the
rem bubble blower and the bonfire and campfire as its own, trash cans and the trash chute as its trash
rem cans, clothing booths as its wardrobe, buffets as a fridge, the sprinkler as the smoke alarm; cakes,
rem balloons, wedding arches, the windmill and pet furniture as decorations; cars, shop displays and
rem pet supplies left out - the GameCube has no cars, shops or pets). The PC buy-mode tabs are then
rem all done, except the Entertainment / recreation objects left out on purpose (bowling, darts, card
rem tables, karaoke, rinks, playground pieces, ...). Delete its folder
rem in mods to remove it (saves stay loadable). Converting again replaces the folders it wrote.
rem Walls and floors: build mode's Wallpaper and Floors tabs list them after the GameCube's own (one
rem folder per PC sub-sort: pcw1_paint ... pcw9_other, pcf1_carpet ... pcf8_other); they paint, sell back
rem and save like the GameCube's. A lot shows at most 160 different converted wallpapers and 188 floors at
rem once (the GameCube keeps a pattern number per wall side and floor tile); delete a folder and lots
rem saved with its coverings show similar GameCube ones instead. They ARE in the default run: about 2100
rem coverings take only half a minute and 40 MB (the objects about three and a half minutes and 280 MB).
rem Takes about four minutes for everything of all packs on two cores (progress per object, with the
rem percentage and the time so far).
setlocal EnableDelayedExpansion
set "PC=Q:\Claude\SIMMY\The Sims 2 Ultimate Collection"
set "ISO=E:\Roms\Gamecube\Sims 2, The (USA).iso"
rem Put this file next to build_simmy.bat (where run_net_host.bat is), or next to simmy.exe.
set "BIN=%~dp0build\RelWithDebInfo"
if exist "%~dp0simmy.exe" set "BIN=%~dp0"
set "TOOL=%~dp0port\tools\pcobj\pcobj_convert.py"
if not exist "%TOOL%" set "TOOL=%~dp0tools\pcobj\pcobj_convert.py"
if not exist "%TOOL%" set "TOOL=%~dp0..\pcobj\pcobj_convert.py"
if not exist "%TOOL%" (echo Cannot find tools\pcobj\pcobj_convert.py & pause & exit /b 1)
python -c "import numpy, PIL" 2>nul || python -m pip install --user numpy Pillow
if /i "%~1"=="list" (
  python "%TOOL%" --pc "%PC%" --list "%~2"
  pause
  exit /b
)
if /i "%~1"=="inventory" (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --categories all --inventory "%BIN%\mods\pcobj_inventory.tsv"
  pause
  exit /b
)
rem category arguments: decorative, surfaces, seating, appliances, lighting, plumbing, electronics, hobbies, storage, kids, misc, walls, floors (any of them, in any order)
set "CATS="
for %%C in (%*) do (
  if /i "%%~C"=="decorative" set "CATS=!CATS!,decorative"
  if /i "%%~C"=="surfaces" set "CATS=!CATS!,surfaces"
  if /i "%%~C"=="seating" set "CATS=!CATS!,seating"
  if /i "%%~C"=="beds" set "CATS=!CATS!,seating"
  if /i "%%~C"=="appliances" set "CATS=!CATS!,appliances"
  if /i "%%~C"=="lighting" set "CATS=!CATS!,lighting"
  if /i "%%~C"=="lamps" set "CATS=!CATS!,lighting"
  if /i "%%~C"=="plumbing" set "CATS=!CATS!,plumbing"
  if /i "%%~C"=="electronics" set "CATS=!CATS!,electronics"
  if /i "%%~C"=="hobbies" set "CATS=!CATS!,hobbies"
  if /i "%%~C"=="storage" set "CATS=!CATS!,storage"
  if /i "%%~C"=="dressers" set "CATS=!CATS!,storage"
  if /i "%%~C"=="kids" set "CATS=!CATS!,kids"
  if /i "%%~C"=="toys" set "CATS=!CATS!,kids"
  if /i "%%~C"=="misc" set "CATS=!CATS!,misc"
  if /i "%%~C"=="miscellaneous" set "CATS=!CATS!,misc"
  if /i "%%~C"=="walls" set "CATS=!CATS!,walls"
  if /i "%%~C"=="wallpaper" set "CATS=!CATS!,walls"
  if /i "%%~C"=="floors" set "CATS=!CATS!,floors"
)
if defined CATS (
  echo Converting every %CATS:~1% object of every pack under "%PC%" ...
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --categories %CATS:~1% --out "%BIN%\mods"
) else if "%~1"=="" (
  echo Converting every buy-mode object ^(Decorative, Surfaces, Seating and Beds, Appliances, Lighting, Plumbing, Electronics, Hobbies, Storage, Kids and Miscellaneous^) and every wall and floor covering of every pack under "%PC%" ...
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --categories decorative,surfaces,seating,appliances,lighting,plumbing,electronics,hobbies,storage,kids,misc,walls,floors --out "%BIN%\mods"
) else if "%~2"=="" (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%~1" --out "%BIN%\mods"
) else (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%~1" --base "%~2" --out "%BIN%\mods"
)
echo.
echo Done (exit code %ERRORLEVEL%). Start the game: the objects are in buy mode, on every tab -
echo Decorative, Surfaces, Seating ^& Beds, Appliances, Lighting, Plumbing, Electronics, Skill and Misc -
echo and the wall and floor coverings in build mode, on the Wallpaper and Floors tabs after the
echo GameCube's own (the C stick pages four columns). With no category given this was the complete set:
echo every PC buy-mode tab the GameCube has a use or a place for, and every wall and floor covering.
echo The lists are mods\pcwall_inventory.tsv, mods\pcfloor_inventory.tsv, mods\pcdeco_inventory.tsv,
echo mods\pcsurf_inventory.tsv, mods\pcseat_inventory.tsv, mods\pcappl_inventory.tsv,
echo mods\pclight_inventory.tsv, mods\pcplumb_inventory.tsv, mods\pcelec_inventory.tsv,
echo mods\pchobby_inventory.tsv, mods\pcstorage_inventory.tsv, mods\pckids_inventory.tsv and
echo mods\pcmisc_inventory.tsv (one row per object: converted or skipped and why, its PC and GameCube tab).
pause
