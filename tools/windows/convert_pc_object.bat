@echo off
rem SIMMY PC objects (PC mod: mods): convert an object from YOUR copy of The Sims 2 (PC) into the mods
rem folder next to simmy.exe. Needs Python 3 (python.org; tick "Add to PATH") - the first run installs
rem numpy and Pillow for it. Nothing is downloaded from EA or anywhere else; see docs\mods.md.
rem
rem   convert_pc_object.bat                              the No-Fuss Ficus (the tested example)
rem   convert_pc_object.bat list ficus                   list PC objects whose name contains "ficus"
rem   convert_pc_object.bat "No-Fuss Ficus" plant_floor_rubbertree
rem                                                      PC object (name or GUID) + GameCube base object
rem
rem Decorative objects work best (plants, sculptures, paintings need a wall base). The new object
rem behaves like its GameCube base. Delete its folder in mods to remove it (saves stay loadable).
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
set "OBJ=ficus"
set "BASE=plant_floor_rubbertree"
if not "%~1"=="" set "OBJ=%~1"
if not "%~2"=="" set "BASE=%~2"
python "%TOOL%" --pc "%PC%" --gc "%ISO%" --object "%OBJ%" --base "%BASE%" --out "%BIN%\mods"
echo.
echo Done (exit code %ERRORLEVEL%). Start the game: the object is in buy mode on its base object's tab.
pause
