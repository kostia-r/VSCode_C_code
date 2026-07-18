@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ============================================================
REM Mophun batch reference capture: Sony Ericsson T610
REM All machine-specific paths are configured here.
REM ============================================================

set "SCRIPT_DIR=%~dp0"

set "GAME_DIR=C:\mophun\MY\MVM\mpn\T610"
set "MOPHUN_EXE=C:\mophun\MY\mophun_2.5.4_tuxality_A2\mophun.exe"
set "AUTOHOTKEY_EXE=C:\Program Files\AutoHotkey\AutoHotkey.exe"
set "FFMPEG_EXE=C:\ffmpeg\bin\ffmpeg.exe"

set "AUDIO_DEVICE=Stereo Mix (Realtek(R) Audio)"
REM Negative = move audio earlier; positive = move audio later.
set "AUDIO_SYNC_MS=0"

set "DURATION=30"
set "DEVICE_NAME=T610"
set "SCREEN_W=128"
set "SCREEN_H=160"
set "ZOOM=2"
set "PROFILE=model2"
set "DATE=2003-10-31"

cd /d "%SCRIPT_DIR%"

echo Running %DEVICE_NAME% batch from: "%GAME_DIR%"
echo Logs will be written to: "%SCRIPT_DIR%logs"
echo.

for %%F in ("%GAME_DIR%\*.mpn") do (
    echo ============================================================
    echo %DEVICE_NAME%: %%~nxF
    echo ============================================================

    py "%SCRIPT_DIR%run_mophun_case.py" "%%~fF" ^
        --mophun-exe "%MOPHUN_EXE%" ^
        --autohotkey-exe "%AUTOHOTKEY_EXE%" ^
        --ffmpeg-exe "%FFMPEG_EXE%" ^
        --duration %DURATION% ^
        --device-name %DEVICE_NAME% ^
        --width %SCREEN_W% ^
        --height %SCREEN_H% ^
        --zoom %ZOOM% ^
        --profile %PROFILE% ^
        --date %DATE% ^
        --record-video ^
        --audio-device "%AUDIO_DEVICE%" ^
        --audio-sync-ms %AUDIO_SYNC_MS%

    echo.
)

echo %DEVICE_NAME% batch finished.
pause
