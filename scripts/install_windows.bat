@echo off
setlocal
chcp 65001 >nul 2>&1

rem Rygostancija installer for Windows. Double-click to run.
rem Administrator rights are needed because the shared VST3 folder is in Program Files.

set "PLUGIN=Rygostancija.vst3"
set "SOURCE=%~dp0%PLUGIN%"
set "DEST=%CommonProgramW6432%"
if not defined DEST set "DEST=%CommonProgramFiles%"
set "DEST=%DEST%\VST3"

if not exist "%SOURCE%\Contents\x86_64-win\%PLUGIN%" goto not_unpacked

fltmc >nul 2>&1
if not errorlevel 1 goto install

rem Ask for rights once, so that a refusal does not keep opening new windows.
if /i "%~1"=="--elevated" goto no_admin
echo Windows will now ask for administrator rights...
set "SELF=%~f0"
powershell -NoProfile -Command "Start-Process -FilePath '%SELF:'=''%' -ArgumentList '--elevated' -Verb RunAs"
if errorlevel 1 goto no_admin
exit /b 0

:install
echo Installing Rygostancija (ZHRI Records)...
if not exist "%DEST%" mkdir "%DEST%"
if exist "%DEST%\%PLUGIN%" rmdir /s /q "%DEST%\%PLUGIN%"
if exist "%DEST%\%PLUGIN%" goto in_use
xcopy /E /I /Y /Q "%SOURCE%" "%DEST%\%PLUGIN%" >nul
if errorlevel 1 goto copy_failed
if not exist "%DEST%\%PLUGIN%\Contents\x86_64-win\%PLUGIN%" goto copy_failed

echo.
echo Done. Restart REAPER and look for the plugin under ZHRI Records.
echo.
pause
exit /b 0

:not_unpacked
echo The %PLUGIN% folder is not next to the installer.
echo Unpack the whole archive first: right-click it and choose Extract All.
goto failed

:no_admin
echo Cannot install without administrator rights: the shared VST3 folder is in Program Files.
goto failed

:in_use
echo Could not replace the old version: the plugin seems to be open.
echo Close REAPER and other audio programs, then run the installer again.
goto failed

:copy_failed
echo Could not copy the plugin to %DEST%.
goto failed

:failed
echo.
pause
exit /b 1
