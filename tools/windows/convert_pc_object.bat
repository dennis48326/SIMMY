@echo off
rem SIMMY PC objects (PC mod: mods): convert objects from YOUR copy of The Sims 2 (PC) - the base game
rem and every expansion and stuff pack installed under it - into the mods folder next to simmy.exe.
rem Needs Python 3 (python.org; tick "Add to PATH") - the first run installs numpy and Pillow for it.
rem Nothing is downloaded from EA or anywhere else; see docs\mods.md.
rem
rem   convert_pc_object.bat                     every Decorative, Surfaces, Seating and Beds, Appliances,
rem                                             Lighting, Plumbing, Electronics, Hobbies, Storage and Kids
rem                                             object of every pack found (default)
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
rem   convert_pc_object.bat decorative seating  several categories
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
rem toddlers). Delete its folder
rem in mods to remove it (saves stay loadable). Converting again replaces the folders it wrote. Takes
rem about three minutes for all packs on two cores (progress per object, with the percentage and the
rem time so far).
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
rem category arguments: decorative, surfaces, seating, appliances, lighting, plumbing, electronics, hobbies, storage, kids (any of them, in any order)
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
)
if defined CATS (
  echo Converting every %CATS:~1% object of every pack under "%PC%" ...
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --categories %CATS:~1% --out "%BIN%\mods"
) else if "%~1"=="" (
  echo Converting every Decorative, Surfaces, Seating and Beds, Appliances, Lighting, Plumbing, Electronics, Hobbies, Storage and Kids object of every pack under "%PC%" ...
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --categories decorative,surfaces,seating,appliances,lighting,plumbing,electronics,hobbies,storage,kids --out "%BIN%\mods"
) else if "%~2"=="" (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%~1" --out "%BIN%\mods"
) else (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%~1" --base "%~2" --out "%BIN%\mods"
)
echo.
echo Done (exit code %ERRORLEVEL%). Start the game: the objects are in buy mode, Decorative,
echo Surfaces, Seating ^& Beds, Appliances, Lighting, Plumbing, Electronics, Skill and Misc tabs (the C
echo stick pages four columns). The lists are mods\pcdeco_inventory.tsv, mods\pcsurf_inventory.tsv,
echo mods\pcseat_inventory.tsv, mods\pcappl_inventory.tsv, mods\pclight_inventory.tsv,
echo mods\pcplumb_inventory.tsv, mods\pcelec_inventory.tsv, mods\pchobby_inventory.tsv,
echo mods\pcstorage_inventory.tsv and mods\pckids_inventory.tsv (with each object's PC and GameCube tab).
pause
