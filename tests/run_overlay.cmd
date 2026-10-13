@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%p in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%p"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cd /d "%~dp0..\build"
cl /nologo /c /O2 /MT /I ..\vendor\minhook-1.3.4\include ..\vendor\minhook-1.3.4\src\buffer.c ..\vendor\minhook-1.3.4\src\hook.c ..\vendor\minhook-1.3.4\src\trampoline.c ..\vendor\minhook-1.3.4\src\hde\hde64.c
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /MT /O2 /W3 /I ..\vendor /I ..\vendor\minhook-1.3.4\include /I ..\vendor\imgui-1.92.9b /I ..\vendor\imgui-1.92.9b\backends ..\tests\overlay_hook_test.cpp ..\vendor\imgui-1.92.9b\imgui.cpp ..\vendor\imgui-1.92.9b\imgui_draw.cpp ..\vendor\imgui-1.92.9b\imgui_tables.cpp ..\vendor\imgui-1.92.9b\imgui_widgets.cpp ..\vendor\imgui-1.92.9b\backends\imgui_impl_win32.cpp ..\vendor\imgui-1.92.9b\backends\imgui_impl_dx11.cpp buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:overlay_hook_test.exe d3d11.lib dxgi.lib xinput.lib d3dcompiler.lib
if errorlevel 1 exit /b 1
overlay_hook_test.exe
exit /b %errorlevel%
