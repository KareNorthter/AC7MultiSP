@echo off
setlocal
call "%~dp0build-ui.cmd"
if errorlevel 1 exit /b 1
call "%~dp0build-core-portable.cmd"
if errorlevel 1 exit /b 1
echo Built UI release DLL and a separate core candidate. The tested dist core is preserved.
exit /b %errorlevel%
