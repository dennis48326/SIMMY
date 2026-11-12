@echo off
rem SIMMY PC objects (PC mod: mods): SCREENSHOT MODE. Starts the game with buy mode showing ONLY the
rem objects converted from The Sims 2 (PC) by convert_pc_object.bat (the mods folder next to simmy.exe):
rem the GameCube's own objects are not for sale in this mode (the ones already on a lot stay there).
rem
rem   show_pc_objects.bat                         the disc image from ISO below (or iso= in simmy.ini)
rem   show_pc_objects.bat "D:\Games\Sims 2.iso"   another disc image
rem
rem Also in this mode, for taking pictures:
rem  - its OWN memory card (the folder pc_objects_card next to simmy.exe): your real saves are never
rem    read or written. Start a Free Play game the first time; save it to come back to the same lot.
rem  - 999,999 simoleons whenever buy mode opens (Sims never run short).
rem  - no fire-code limit (place as many objects as you like).
rem Nothing in simmy.ini changes: start simmy.exe (or build_simmy.bat's way) as usual for normal play.
rem Screenshots: Win+Alt+PrtScn (Xbox Game Bar, saved in Videos\Captures) or Win+Shift+S (Snipping Tool).
setlocal
set "ISO=E:\Roms\Gamecube\Sims 2, The (USA).iso"
if not "%~1"=="" set "ISO=%~1"
rem Put this file next to build_simmy.bat (where convert_pc_object.bat is), or next to simmy.exe.
set "BIN=%~dp0build\RelWithDebInfo"
if exist "%~dp0simmy.exe" set "BIN=%~dp0"
if not exist "%BIN%\simmy.exe" (echo Cannot find simmy.exe in "%BIN%" - run build_simmy.bat first & pause & exit /b 1)
if not exist "%BIN%\mods" (echo No mods folder in "%BIN%" - run convert_pc_object.bat first & pause & exit /b 1)
cd /d "%BIN%"
set "ISOARG="
if exist "%ISO%" set ISOARG=--iso "%ISO%"
set "SIMMY_MODS=%BIN%\mods"
set SIMMY_MODS_ONLY=1
set SIMMY_FIRE_CODE_LIMIT=off
set SIMMY_BUY_FUNDS=999999
set "SIMMY_CARD_DIR=%BIN%\pc_objects_card"
echo Starting SIMMY in PC objects screenshot mode (only converted objects in buy mode) ...
simmy.exe %ISOARG% --split=gc 2> "%~dp0show_pc_objects_log.txt"
echo.
echo simmy.exe exited with code %ERRORLEVEL% (log: show_pc_objects_log.txt)
pause
