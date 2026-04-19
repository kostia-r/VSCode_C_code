@echo off
setlocal

pushd "%~dp0"
set "MAKE=C:\mingw64\bin\mingw32-make.exe"

if not exist "%MAKE%" (
    echo Make not found: %MAKE%
    popd
    exit /b 1
)

"%MAKE%" rebuild
if errorlevel 1 (
    set "STATUS=%ERRORLEVEL%"
    popd
    exit /b %STATUS%
)

for %%I in ("%CD%") do set "APP_NAME=%%~nxI"
set "EXE=%CD%\%APP_NAME%.exe"

if not exist "%EXE%" (
    echo Executable not found: %EXE%
    popd
    exit /b 1
)

"%EXE%"
set "STATUS=%ERRORLEVEL%"

popd
exit /b %STATUS%
