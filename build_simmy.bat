@echo off
rem Builds the SIMMY PC port: unpacks simmy_port.zip next to this script, configures with CMake
rem (newest Visual Studio, x64), builds simmy.exe and SimmyLauncher.exe, runs the renderer test and
rem starts the launcher. Everything is logged to build_log.txt in this folder.
setlocal
cd /d "%~dp0"
set LOG=%~dp0build_log.txt
set SRC=%~dp0port
set BLD=%~dp0build
echo SIMMY build started %DATE% %TIME% > "%LOG%"

echo === tools === >> "%LOG%"
rem Visual Studio (for its bundled CMake and its C++ runtime DLLs).
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath > "%TEMP%\simmy_vsdir.txt" 2>nul
set /p VSDIR=<"%TEMP%\simmy_vsdir.txt"
echo VSDIR=%VSDIR% >> "%LOG%"
rem cmake: on PATH, else the copy that ships with Visual Studio, else install it with pip.
where cmake >nul 2>&1 && goto :cmakeok
for /f "delims=" %%i in ('python -c "import sysconfig;print(sysconfig.get_path('scripts','nt_user'))"') do set "PYUSERSCRIPTS=%%i"
if exist "%PYUSERSCRIPTS%\cmake.exe" set "PATH=%PYUSERSCRIPTS%;%PATH%"
where cmake >nul 2>&1 && goto :cmakeok
set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where cmake >nul 2>&1 && goto :cmakeok
echo Installing CMake with pip... >> "%LOG%"
echo Installing CMake with pip...
python -m pip install --user --upgrade cmake >> "%LOG%" 2>&1
for /f "delims=" %%i in ('python -c "import sysconfig;print(sysconfig.get_path('scripts','nt_user'))"') do set "PYUSERSCRIPTS=%%i"
set "PATH=%PYUSERSCRIPTS%;%PATH%"
:cmakeok
where cmake >> "%LOG%" 2>&1 || (echo MISSING: cmake >> "%LOG%")
cmake --version >> "%LOG%" 2>&1
where git >> "%LOG%" 2>&1 || (echo MISSING: git >> "%LOG%")
where python >> "%LOG%" 2>&1
python --version >> "%LOG%" 2>&1
where py >> "%LOG%" 2>&1
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property displayName >> "%LOG%" 2>&1

echo === unpack === >> "%LOG%"
rem A previous test run that is still open (or stuck) keeps simmy.exe locked.
taskkill /f /im simmy.exe >nul 2>&1
rem An open launcher keeps SimmyLauncher.exe locked the same way.
taskkill /f /im SimmyLauncher.exe >nul 2>&1
if exist "%SRC%" rmdir /s /q "%SRC%"
tar -xf simmy_port.zip >> "%LOG%" 2>&1
if not exist "%SRC%\CMakeLists.txt" (echo UNPACK FAILED >> "%LOG%" & goto :done)

echo === configure === >> "%LOG%"
echo Configuring (first time downloads aurora, Dawn and SDL; this can take a while)...
cmake -S "%SRC%" -B "%BLD%" -A x64 >> "%LOG%" 2>&1
if errorlevel 1 (echo CONFIGURE FAILED >> "%LOG%" & goto :done)

echo === build === >> "%LOG%"
echo Building...
cmake --build "%BLD%" --config RelWithDebInfo --target simmy simmy_launcher -- /m /v:minimal >> "%LOG%" 2>&1
if errorlevel 1 (echo BUILD FAILED >> "%LOG%" & goto :done)

rem The exe is built with this Visual Studio's C++ library; ship that runtime next to it so an older
rem system-wide VC++ redistributable cannot break startup ("entry point not found").
for /d %%d in ("%VSDIR%\VC\Redist\MSVC\*") do for /d %%c in ("%%d\x64\Microsoft.VC*.CRT") do copy /y "%%c\*.dll" "%BLD%\RelWithDebInfo\" >> "%LOG%" 2>&1
for /d %%d in ("%VSDIR%\VC\Tools\MSVC\*") do if exist "%%d\bin\Hostx64\x64\dumpbin.exe" set "DUMPBIN=%%d\bin\Hostx64\x64\dumpbin.exe"
if defined DUMPBIN "%DUMPBIN%" /nologo /dependents "%BLD%\RelWithDebInfo\simmy.exe" >> "%LOG%" 2>&1
dir /b "%BLD%\RelWithDebInfo\*.dll" >> "%LOG%" 2>&1
echo === startup probes === >> "%LOG%"
cmake --build "%BLD%" --config RelWithDebInfo --target probe_plain probe_sdl probe_dawn -- /m /v:minimal >> "%LOG%" 2>&1
for %%e in (probe_plain probe_sdl probe_dawn) do (
  copy /y "%BLD%\RelWithDebInfo\*.dll" "%BLD%\RelWithDebInfo" >nul 2>&1
  powershell -NoProfile -Command "$p = Start-Process -FilePath '%BLD%\RelWithDebInfo\%%e.exe' -RedirectStandardOutput '%~dp0probe_out.txt' -WindowStyle Hidden -PassThru; if (-not $p.WaitForExit(15000)) { '%%e: HUNG'; Stop-Process -Id $p.Id -Force } else { '%%e: exited' }; Get-Content '%~dp0probe_out.txt'" >> "%LOG%" 2>&1
)
echo === render test === >> "%LOG%"
rem The tests read the game from the disc chosen in SimmyLauncher (iso= in simmy.ini, passed as
rem --iso), else from an unpacked game folder one level up (--data, if it has a DATA folder).
rem Without either they are skipped. PowerShell gets the path from the environment (TESTPATH), in
rem quotes, so spaces, brackets and apostrophes in it are fine.
set "ISO="
set "TESTFLAG="
set "TESTPATH="
set "INI=%BLD%\RelWithDebInfo\simmy.ini"
rem simmy.ini is UTF-8: read it with the UTF-8 code page so paths with accents survive.
for /f "tokens=2 delims=:." %%c in ('chcp') do set "OLDCP=%%c"
chcp 65001 >nul
if exist "%INI%" for /f "usebackq tokens=1,* delims==" %%a in (`findstr /r /i /c:"^ *iso *=" "%INI%"`) do set "ISO=%%b"
chcp %OLDCP% >nul
if defined ISO for /f "tokens=* delims= " %%x in ("%ISO%") do set "ISO=%%x"
if defined ISO if exist "%ISO%" set "TESTFLAG=--iso" & set "TESTPATH=%ISO%"
if defined ISO if not defined TESTFLAG >> "%LOG%" echo "The disc in simmy.ini was not found: %ISO%"
if not defined TESTFLAG if exist "%~dp0..\DATA\" set "TESTFLAG=--data" & set "TESTPATH=%~dp0.."
if not defined TESTFLAG (
  echo No disc chosen yet: skipping the headless run and the render test. Choose your disc in SimmyLauncher. >> "%LOG%"
  echo No disc chosen yet: skipping the renderer test.
  goto :tests_done
)
>> "%LOG%" echo "test data: %TESTFLAG% %TESTPATH%"
echo Running the renderer test...
if exist "%BLD%\RelWithDebInfo\simmy_crash.txt" del "%BLD%\RelWithDebInfo\simmy_crash.txt"
echo --- headless run --- >> "%LOG%"
powershell -NoProfile -Command "try { 'SmartAppControl: ' + (Get-MpComputerStatus).SmartAppControlState } catch { 'SmartAppControl: unknown' }" >> "%LOG%" 2>&1
set SIMMY_WATCHDOG=12
rem Own window, no shared console: Start-Process with a hard timeout.
powershell -NoProfile -Command "$p = Start-Process -FilePath '%BLD%\RelWithDebInfo\simmy.exe' -ArgumentList $env:TESTFLAG,([char]34 + $env:TESTPATH + [char]34),'--headless','--frames','30' -RedirectStandardOutput '%~dp0headless_out.txt' -RedirectStandardError '%~dp0headless_log.txt' -WindowStyle Hidden -PassThru; $h = $p.Handle; if (-not $p.WaitForExit(20000)) { 'headless: still running after 20 s'; $p.Threads | ForEach-Object { 'thread ' + $_.Id + ' ' + $_.ThreadState + ' ' + $_.WaitReason }; Stop-Process -Id $p.Id -Force } else { 'headless exit code 0x' + $p.ExitCode.ToString('X8') }" >> "%LOG%" 2>&1
type "%~dp0headless_out.txt" >> "%LOG%" 2>nul
type "%~dp0headless_log.txt" >> "%LOG%" 2>nul
if exist "%BLD%\RelWithDebInfo\simmy_hang.txt" (echo --- headless hang report --- >> "%LOG%" & type "%BLD%\RelWithDebInfo\simmy_hang.txt" >> "%LOG%" & del "%BLD%\RelWithDebInfo\simmy_hang.txt")
if exist "%BLD%\RelWithDebInfo\simmy_crash.txt" (echo --- headless crash report --- >> "%LOG%" & type "%BLD%\RelWithDebInfo\simmy_crash.txt" >> "%LOG%" & del "%BLD%\RelWithDebInfo\simmy_crash.txt")
set SIMMY_WATCHDOG=40
echo --- recent application crashes (event log) --- >> "%LOG%"
powershell -NoProfile -Command "Get-WinEvent -FilterHashtable @{LogName='Application'; Id=1000} -MaxEvents 3 -ErrorAction SilentlyContinue | ForEach-Object { $_.TimeCreated.ToString() + ' ' + ($_.Message -replace '\s+',' ') }" >> "%LOG%" 2>&1
if exist "%~dp0render_test.bmp" del "%~dp0render_test.bmp"
echo --- render test --- >> "%LOG%"
set SIMMY_TEST_CLEAR=1
powershell -NoProfile -Command "$p = Start-Process -FilePath '%BLD%\RelWithDebInfo\simmy.exe' -ArgumentList $env:TESTFLAG,([char]34 + $env:TESTPATH + [char]34),'--render-test','--screenshot','120','%~dp0render_test.bmp','--frames','150' -RedirectStandardOutput '%~dp0run_out.txt' -RedirectStandardError '%~dp0run_log.txt' -PassThru; $h = $p.Handle; if (-not $p.WaitForExit(60000)) { 'render test: still running after 60 s'; Stop-Process -Id $p.Id -Force } else { 'render test exit code 0x' + $p.ExitCode.ToString('X8') }" >> "%LOG%" 2>&1
set SIMMY_TEST_CLEAR=
if exist "%~dp0render_test.bmp" (echo screenshot saved >> "%LOG%") else (echo no screenshot >> "%LOG%")
if exist "%BLD%\RelWithDebInfo\simmy_crash.txt" (echo --- crash report --- >> "%LOG%" & type "%BLD%\RelWithDebInfo\simmy_crash.txt" >> "%LOG%")
if exist "%BLD%\RelWithDebInfo\simmy_hang.txt" (echo --- hang report --- >> "%LOG%" & type "%BLD%\RelWithDebInfo\simmy_hang.txt" >> "%LOG%")
:tests_done
echo BUILD OK >> "%LOG%"
rem Start the launcher once the build succeeds (set SIMMY_NO_LAUNCH=1 to skip). It replaces
rem run_simmy.bat and run_simmy_coop.bat: pick the disc, then Play or Co-op.
rem The test settings above must not reach the game the launcher starts (the watchdog would
rem write simmy_hang.txt 40 s into every game).
set SIMMY_WATCHDOG=
set SIMMY_TEST_CLEAR=
set TESTFLAG=
set TESTPATH=
if not defined SIMMY_NO_LAUNCH start "" "%BLD%\RelWithDebInfo\SimmyLauncher.exe"

:done
echo SIMMY build finished %DATE% %TIME% >> "%LOG%"
echo Done. See build_log.txt
endlocal
