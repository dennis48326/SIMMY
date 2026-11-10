@echo off
rem SIMMY PC objects (PC mod: mods): convert objects from YOUR copy of The Sims 2 (PC) - the base game
rem and every expansion and stuff pack installed under it - into the mods folder next to simmy.exe.
rem Needs Python 3 (python.org; tick "Add to PATH") - the first run installs numpy and Pillow for it.
rem Nothing is downloaded from EA or anywhere else; see docs\mods.md.
rem
rem   convert_pc_object.bat                     every Decorative object of every pack found (default)
rem   convert_pc_object.bat list ficus          list PC objects whose name contains "ficus"
rem   convert_pc_object.bat inventory           write the decorative inventory (mods\pcdeco_inventory.tsv) only
rem   convert_pc_object.bat "No-Fuss Ficus"     one object (name or GUID); the GameCube base is chosen
rem   convert_pc_object.bat "No-Fuss Ficus" plant_floor_rubbertree     ... with this GameCube base object
rem
rem Each object behaves like its GameCube base (chosen by where it goes: wall, floor, table top,
rem ceiling, rug). Delete its folder in mods to remove it (saves stay loadable). Converting again
rem replaces the folders it wrote. Takes a few minutes for all packs.
setlocal
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
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --inventory "%BIN%\mods\pcdeco_inventory.tsv"
  pause
  exit /b
)
if "%~1"=="" (
  echo Converting every Decorative object of every pack under "%PC%" ...
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --all-decorative --out "%BIN%\mods"
) else if "%~2"=="" (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%~1" --out "%BIN%\mods"
) else (
  python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%~1" --base "%~2" --out "%BIN%\mods"
)
echo.
echo Done (exit code %ERRORLEVEL%). Start the game: the objects are in buy mode, Decorative tab
echo (up / down past a column's end pages four columns). The list is in mods\pcdeco_inventory.tsv.
pause
