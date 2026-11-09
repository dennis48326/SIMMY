@echo off
rem SIMMY network play (PC mod): JOIN = player 2. The other PC starts run_net_host.bat first.
rem
rem   run_net_join.bat 192.168.1.20                          join that PC (port 27450)
rem   run_net_join.bat 192.168.1.20:27451 "D:\Sims 2.iso"     another port, another disc image
rem   run_net_join.bat                                       asks for the address
rem
rem Both PCs need the same SIMMY build and the same disc image (the game checks and says what differs).
rem The host's memory card is used for the session (a copy in %%APPDATA%%\SIMMY\net_session); your
rem own memory card is not touched. If the two PCs go out of step, both stop and write files to
rem %%APPDATA%%\SIMMY\net_session: please send that folder from both PCs. See docs\netplay.md.
setlocal
set "ISO=E:\Roms\Gamecube\Sims 2, The (USA).iso"
set "HOST=%~1"
if not "%~2"=="" set "ISO=%~2"
if "%HOST%"=="" set /p "HOST=The host PC's address (e.g. 192.168.1.20): "
if "%HOST%"=="" exit /b 1
rem Put this file next to build_simmy.bat (where run_simmy_coop.bat is), or next to simmy.exe.
set "BIN=%~dp0build\RelWithDebInfo"
if exist "%~dp0simmy.exe" set "BIN=%~dp0"
cd /d "%BIN%"
rem Without the disc image at ISO, simmy.exe uses iso= from simmy.ini (what SimmyLauncher chose).
set "ISOARG="
if exist "%ISO%" set ISOARG=--iso "%ISO%"
echo Joining %HOST% as player 2 ...
simmy.exe %ISOARG% --join %HOST% > "%~dp0net_join_out.txt" 2> "%~dp0net_join_log.txt"
echo.
echo simmy.exe exited with code %ERRORLEVEL% (log: net_join_log.txt)
pause
