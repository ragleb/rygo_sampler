@echo off
setlocal

:: ── Rygo Sampler — Windows Installer ──────────────────────────────────────
::  Copies rygo_sampler.vst3 to C:\Program Files\Common Files\VST3\
::  Copies samples to %USERPROFILE%\Documents\Rygo\Samples\
::  Requests administrator elevation automatically if needed.

:: Check for admin rights; if missing, relaunch elevated
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb runAs"
    exit /b
)

set "PLUGIN_DIR=%~dp0rygo_sampler.vst3"
set "SAMPLES_DIR=%~dp0Samples"
set "DEST=%CommonProgramFiles%\VST3"
set "SAMPLES_DEST=%USERPROFILE%\Documents\Rygo\Samples"

echo.
echo === Rygo Sampler Installer ===
echo.

if not exist "%PLUGIN_DIR%" (
    echo ERROR: rygo_sampler.vst3 not found next to this installer.
    pause
    exit /b 1
)

if not exist "%DEST%" mkdir "%DEST%"

if exist "%DEST%\rygo_sampler.vst3" (
    echo Removing old version...
    rmdir /s /q "%DEST%\rygo_sampler.vst3"
)

echo Installing plugin to %DEST% ...
xcopy /E /I /Y /Q "%PLUGIN_DIR%" "%DEST%\rygo_sampler.vst3\"

:: Install samples (skip files that already exist with /D flag)
if exist "%SAMPLES_DIR%" (
    echo Installing samples to %SAMPLES_DEST% ...
    if not exist "%SAMPLES_DEST%" mkdir "%SAMPLES_DEST%"
    xcopy /Y /Q "%SAMPLES_DIR%\*.wav" "%SAMPLES_DEST%\"
    echo Samples installed.
) else (
    echo [skip] Samples folder not found next to installer.
)

echo.
echo Done! Plugin and samples installed.
echo Restart your DAW to see Rygo Sampler.
echo.
pause
