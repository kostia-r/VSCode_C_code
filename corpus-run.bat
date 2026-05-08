@echo off
setlocal

pushd "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\corpus-run.ps1" %*
set "STATUS=%ERRORLEVEL%"
popd

exit /b %STATUS%
