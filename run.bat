@echo off
setlocal

pushd "%~dp0"

for %%I in ("%CD%") do set "APP_NAME=%%~nxI"
set "EXE=%CD%\%APP_NAME%.exe"
set "DEFAULT_MPN=%CD%\mpn\45596_decrypted.mpn"

if not exist "%EXE%" (
    echo Executable not found: %EXE%
    popd
    exit /b 1
)

if "%~1"=="" (
    if not exist "%DEFAULT_MPN%" (
        echo MPN file not found: %DEFAULT_MPN%
        popd
        exit /b 1
    )

    "%EXE%" "%DEFAULT_MPN%"
) else (
    "%EXE%" %*
)
set "STATUS=%ERRORLEVEL%"

popd
exit /b %STATUS%
