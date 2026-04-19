@echo off
setlocal

pushd "%~dp0"
set "MAKE=C:\mingw64\bin\mingw32-make.exe"

if not exist "%MAKE%" (
    echo Make not found: %MAKE%
    popd
    exit /b 1
)

"%MAKE%" clean
set "STATUS=%ERRORLEVEL%"

popd
exit /b %STATUS%
