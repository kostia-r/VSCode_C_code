@echo off
setlocal

pushd "%~dp0"

for %%I in ("%CD%") do set "APP_NAME=%%~nxI"
set "EXE=%CD%\%APP_NAME%.exe"

if not exist "%EXE%" (
    echo Executable not found: %EXE%
    popd
    exit /b 1
)

"%EXE%" %*
set "STATUS=%ERRORLEVEL%"

popd
exit /b %STATUS%
