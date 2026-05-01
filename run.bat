@echo off
setlocal

pushd "%~dp0"

set "SDL2_BIN=%CD%\SDL2-2.32.6\x86_64-w64-mingw32\bin"
set "DEFAULT_PROFILE=SE_T310"
set "LOG_FILE=%CD%\Build\mvm.log"
if exist "%SDL2_BIN%\SDL2.dll" (
    set "PATH=%SDL2_BIN%;%PATH%"
)

if not exist "%CD%\Build" (
    mkdir "%CD%\Build"
)

for %%I in ("%CD%") do set "APP_NAME=%%~nxI"
set "EXE=%CD%\%APP_NAME%.exe"
set "DEFAULT_MPN=%CD%\mpn\45596_decrypted.mpn"

if not exist "%EXE%" (
    echo Executable not found: %EXE%
    popd
    exit /b 1
)

if "%~1"=="" goto run_default

if /I "%~1"=="SE_T310" goto run_default_with_profile
if /I "%~1"=="SE_T610" goto run_default_with_profile

if exist "%~1" goto run_explicit

:run_default
if not exist "%DEFAULT_MPN%" (
    echo MPN file not found: %DEFAULT_MPN%
    popd
    exit /b 1
)

call :run_with_logging "%DEFAULT_MPN%" %DEFAULT_PROFILE% %1 %2 %3
goto finish

:run_default_with_profile
if not exist "%DEFAULT_MPN%" (
    echo MPN file not found: %DEFAULT_MPN%
    popd
    exit /b 1
)

call :run_with_logging "%DEFAULT_MPN%" %1 %2 %3
goto finish

:run_explicit
call :run_with_logging %*
goto finish

:run_with_logging
powershell -NoProfile -Command "& { & '%EXE%' %* 2>&1 | Tee-Object -FilePath '%LOG_FILE%'; exit $LASTEXITCODE }"
exit /b %ERRORLEVEL%

:finish
set "STATUS=%ERRORLEVEL%"

popd
exit /b %STATUS%
