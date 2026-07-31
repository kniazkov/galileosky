@echo off
setlocal

call "%~dp0build_windows.cmd"
if errorlevel 1 exit /b %ERRORLEVEL%

"%~dp0..\build\windows-desktop\galileosky_test_project.exe"
exit /b %ERRORLEVEL%
