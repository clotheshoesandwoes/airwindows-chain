@echo off
rem Copies the built plugin into the standard plugin folders, where FL Studio finds it.
rem Windows asks for admin rights once. Close FL Studio first, or the old copy is locked.
setlocal

net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

set "SRC=%~dp0..\build\AirwindowsChain_artefacts\Release"
if not exist "%SRC%\VST3\Airwindows Chain.vst3" (
    echo Build the plugin first: tools\build.bat
    pause
    exit /b 1
)

robocopy "%SRC%\VST3\Airwindows Chain.vst3" "%CommonProgramFiles%\VST3\Airwindows Chain.vst3" /MIR /NJH /NJS /NDL /NFL /NP >nul
if %errorlevel% geq 8 (
    echo Could not copy the VST3. Is FL Studio still open?
    pause
    exit /b 1
)
if exist "%SRC%\CLAP\Airwindows Chain.clap" (
    if not exist "%CommonProgramFiles%\CLAP" mkdir "%CommonProgramFiles%\CLAP"
    copy /Y "%SRC%\CLAP\Airwindows Chain.clap" "%CommonProgramFiles%\CLAP\Airwindows Chain.clap" >nul
)

echo Installed:
echo   %CommonProgramFiles%\VST3\Airwindows Chain.vst3
echo   %CommonProgramFiles%\CLAP\Airwindows Chain.clap
echo.
echo In FL Studio: Options, Manage plugins, Find more plugins. It shows up under Kani.
pause
