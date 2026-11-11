@echo off
rem SIMMY PC objects (PC mod: mods): convert objects from YOUR copy of The Sims 2 (PC) - the base game
rem and every expansion and stuff pack installed under it - into the mods folder next to simmy.exe.
rem Needs Python 3 (python.org; tick "Add to PATH") - the first run installs numpy and Pillow for it.
rem Nothing is downloaded from EA or anywhere else; see docs\mods.md.
rem
rem   convert_pc_object.bat                     every Decorative, Surfaces, Seating and Beds object of every pack found (default)
rem   convert_pc_object.bat decorative          only the Decorative objects
rem   convert_pc_object.bat surfaces            only the Surfaces objects (counters, tables, desks, ...)
rem   convert_pc_object.bat seating             only the Seating objects and Beds (chairs, sofas, stools, beds, ...)
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
rem PC seat's height). Delete its folder in mods to remove it (saves stay loadable). Converting again
rem replaces the folders it wrote. Takes about two minutes for all packs on two cores (progress per
rem object, with the percentage and the time so far).
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
rem category arguments: decorative, surfaces, seating (any of them, in any order)
set "CATS="
for %%C in (%*) do (
  if /i "%%~C"=="decorative" set "CATS=!CATS!,decorative"
  if /i "%%~C"=="surfaces" set "CATS=!CATS!,surfaces"
  if /i "%%~C"=="seating" set "CATS=!CATS!,seating"
  if /i "%%~C"=="beds" set "CATS=!CATS!,seating"
)
if defined CATS (
  echo Converting every %CATS:~1% object of every pack under "%PC%" ...
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --categories %CATS:~1% --out "%BIN%\mods"
) else if "%~1"=="" (
  echo Converting every Decorative, Surfaces, Seating and Beds object of every pack under "%PC%" ...
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --categories decorative,surfaces,seating --out "%BIN%\mods"
) else if "%~2"=="" (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%~1" --out "%BIN%\mods"
) else (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%~1" --base "%~2" --out "%BIN%\mods"
)
echo.
echo Done (exit code %ERRORLEVEL%). Start the game: the objects are in buy mode, Decorative,
echo Surfaces and Seating ^& Beds tabs (the C stick pages four columns). The lists are
echo mods\pcdeco_inventory.tsv, mods\pcsurf_inventory.tsv and mods\pcseat_inventory.tsv.
pause
