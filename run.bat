@echo off
setlocal

pushd "%~dp0"

set "SDL2_BIN=%CD%\SDL2-2.32.6\x86_64-w64-mingw32\bin"
set "DEFAULT_PROFILE=SE_T310"
rem set "DEFAULT_PROFILE=SE_T610"
set "DEFAULT_FIXED_DATE_TIME=2003-11-04T12:00:00"
set "DEFAULT_MAX_STEPS=100000000"
set "DEFAULT_MAX_LOGGED_CALLS=0"
if exist "%SDL2_BIN%\SDL2.dll" (
    set "PATH=%SDL2_BIN%;%PATH%"
)

if not exist "%CD%\Build" (
    mkdir "%CD%\Build"
)

for %%I in ("%CD%") do set "APP_NAME=%%~nxI"
set "EXE=%CD%\%APP_NAME%.exe"
set "DEFAULT_MPN=%CD%\mpn\T310\DeepAbyss_v.1.4_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\DeepAbyss_v.1.1_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T610\DeepAbyss_v.1.4_T610_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T610\VRally_T610_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\VRally2_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\Minigolf_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\Prehistorik_Man_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\SynergenixRally_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\Alien Scum_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\alphaattack_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\Bouncy_demo_T310_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T310\HoneyCave_T310_decrypted.mpn"

rem set "DEFAULT_MPN=%CD%\mpn\T610\CMRally4_T610_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T610\SpaceExplorer_T610_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T610\jbubble_T610_decrypted.mpn"
rem set "DEFAULT_MPN=%CD%\mpn\T610\IcebloxPlus610_T610_decrypted.mpn"

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

call :run_with_logging "%DEFAULT_MPN%" %DEFAULT_PROFILE% %DEFAULT_MAX_STEPS% %DEFAULT_MAX_LOGGED_CALLS% --fixed-date-time %DEFAULT_FIXED_DATE_TIME%
goto finish

:run_default_with_profile
if not exist "%DEFAULT_MPN%" (
    echo MPN file not found: %DEFAULT_MPN%
    popd
    exit /b 1
)

call :args_have_fixed_date %*
if "%~2"=="" (
    if "%HAS_FIXED_DATE%"=="1" (
        call :run_with_logging "%DEFAULT_MPN%" %* %DEFAULT_MAX_STEPS% %DEFAULT_MAX_LOGGED_CALLS%
    ) else (
        call :run_with_logging "%DEFAULT_MPN%" %* %DEFAULT_MAX_STEPS% %DEFAULT_MAX_LOGGED_CALLS% --fixed-date-time %DEFAULT_FIXED_DATE_TIME%
    )
) else (
    if "%HAS_FIXED_DATE%"=="1" (
        call :run_with_logging "%DEFAULT_MPN%" %*
    ) else (
        call :run_with_logging "%DEFAULT_MPN%" %* --fixed-date-time %DEFAULT_FIXED_DATE_TIME%
    )
)
goto finish

:run_explicit
call :args_have_fixed_date %*
if "%HAS_FIXED_DATE%"=="1" (
    call :run_with_logging %*
) else (
    call :run_with_logging %* --fixed-date-time %DEFAULT_FIXED_DATE_TIME%
)
goto finish

:args_have_fixed_date
set "HAS_FIXED_DATE=0"
:args_have_fixed_date_loop
if "%~1"=="" exit /b 0
if /I "%~1"=="--fixed-date-time" (
    set "HAS_FIXED_DATE=1"
    exit /b 0
)
shift
goto args_have_fixed_date_loop

:run_with_logging
"%EXE%" %*
exit /b %ERRORLEVEL%

:finish
set "STATUS=%ERRORLEVEL%"

popd
exit /b %STATUS%
