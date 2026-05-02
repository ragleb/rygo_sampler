@echo off
setlocal

:: ── Rygo Sampler — Windows Installer ──────────────────────────────────────
::  Copies rygo_sampler.vst3 to C:\Program Files\Common Files\VST3\
::  Requests administrator elevation automatically if needed.

:: Check for admin rights; if missing, relaunch elevated
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb runAs"
    exit /b
)

set "PLUGIN_DIR=%~dp0rygo_sampler.vst3"
set "DEST=%CommonProgramFiles%\VST3"

echo.
echo === Rygo Sampler Installer ===
echo.

if not exist "%PLUGIN_DIR%" (
    echo ERROR: rygo_sampler.vst3 not found next to this installer.
    echo Make sure install.bat is in the same folder as rygo_sampler.vst3
    pause
    exit /b 1
)

if not exist "%DEST%" (
    mkdir "%DEST%"
)

:: Remove old version if present
if exist "%DEST%\rygo_sampler.vst3" (
    echo Removing old version...
    rmdir /s /q "%DEST%\rygo_sampler.vst3"
)

echo Installing to %DEST% ...
xcopy /E /I /Y /Q "%PLUGIN_DIR%" "%DEST%\rygo_sampler.vst3\"

if %errorlevel% equ 0 (
    echo.
    echo Done! Plugin installed to:
    echo   %DEST%\rygo_sampler.vst3
    echo.
    echo Restart your DAW to see Rygo Sampler.
) else (
    echo.
    echo ERROR: Installation failed. Try running as Administrator manually.
)

echo.
pause
