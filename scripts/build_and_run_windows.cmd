@echo off
setlocal

pushd "%~dp0.."

cmake --preset windows-desktop-release
if errorlevel 1 goto error

cmake --build --preset windows-desktop-release
if errorlevel 1 goto error

"%CD%\build\windows-desktop\galileosky_test_project.exe"
set "result=%ERRORLEVEL%"
popd
exit /b %result%

:error
set "result=%ERRORLEVEL%"
popd
exit /b %result%
