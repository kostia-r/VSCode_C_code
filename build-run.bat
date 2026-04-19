@echo off
setlocal

pushd "%~dp0"
set "MAKE=C:\mingw64\bin\mingw32-make.exe"

if not exist "%MAKE%" (
    echo Make not found: %MAKE%
    popd
    exit /b 1
)

"%MAKE%"
if errorlevel 1 (
    set "STATUS=%ERRORLEVEL%"
    popd
    exit /b %STATUS%
)

"%~dp0run.bat" %*
set "STATUS=%ERRORLEVEL%"

if not "%STATUS%"=="0" (
    popd
    exit /b %STATUS%
)

popd
exit /b %STATUS%
