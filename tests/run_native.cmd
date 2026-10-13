@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%p in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%p"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cd /d "%~dp0..\build"
cl /nologo /std:c++17 /utf-8 /EHsc /MT /W4 ..\tests\reload_timer_test.cpp /Fe:reload_timer_test.exe
if errorlevel 1 exit /b 1
reload_timer_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /MT /W3 /I ..\vendor ..\tests\weapon_group_test.cpp /Fe:weapon_group_test.exe
if errorlevel 1 exit /b 1
weapon_group_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /MT /W4 ..\tests\panel_pointer_test.cpp /Fe:panel_pointer_test.exe
if errorlevel 1 exit /b 1
panel_pointer_test.exe
exit /b %errorlevel%
