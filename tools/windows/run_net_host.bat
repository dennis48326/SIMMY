@echo off
rem SIMMY network play (PC mod): HOST = player 1. Start this first; the other PC then runs
rem run_net_join.bat with this PC's address (shown below and in the game window's title).
rem
rem   run_net_host.bat                         the disc image from ISO below (or iso= in simmy.ini)
rem   run_net_host.bat "D:\Games\Sims 2.iso"   another disc image
rem
rem Both PCs need the same SIMMY build and the same disc image (the game checks and says what differs).
rem The first time, Windows asks whether simmy.exe may use the network: allow it for private networks
rem (else the other PC cannot connect). The port is 27450 (TCP); change PORT below on both PCs if needed.
rem Your memory card is the session's: the other PC gets a copy, and saving in the game saves here.
rem If the two PCs ever go out of step, both stop and write files to %%APPDATA%%\SIMMY\net_session:
rem please send that folder from both PCs. See docs\netplay.md.
setlocal
set "ISO=E:\Roms\Gamecube\Sims 2, The (USA).iso"
if not "%~1"=="" set "ISO=%~1"
set PORT=27450
rem Put this file next to build_simmy.bat (where run_simmy_coop.bat is), or next to simmy.exe.
set "BIN=%~dp0build\RelWithDebInfo"
if exist "%~dp0simmy.exe" set "BIN=%~dp0"
cd /d "%BIN%"
rem Without the disc image at ISO, simmy.exe uses iso= from simmy.ini (what SimmyLauncher chose).
set "ISOARG="
if exist "%ISO%" set ISOARG=--iso "%ISO%"
echo This PC's addresses (the other player needs one of the IPv4 ones of your network, e.g. 192.168.x.x):
ipconfig | findstr /c:"IPv4"
echo.
echo Starting simmy.exe as the host (player 1) on port %PORT% ...
simmy.exe %ISOARG% --host %PORT% > "%~dp0net_host_out.txt" 2> "%~dp0net_host_log.txt"
echo.
echo simmy.exe exited with code %ERRORLEVEL% (log: net_host_log.txt)
pause
