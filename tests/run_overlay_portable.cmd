@echo off
setlocal
if not defined DUALSP_TOOLCHAIN set "DUALSP_TOOLCHAIN=%~dp0..\tools\llvm-mingw-20261006-ucrt-x86_64"
call "%~dp0..\src\build-ui.cmd"
if errorlevel 1 exit /b 1
cd /d "%~dp0..\build-ui"
set "TMP=%CD%\..\Temp\compiler"
set "TEMP=%TMP%"
"%DUALSP_TOOLCHAIN%\bin\clang++.exe" -std=c++17 -O2 -static -I ../vendor -I ../vendor/minhook-1.3.4/include -I ../vendor/imgui-1.92.9b -I ../vendor/imgui-1.92.9b/backends ../tests/overlay_hook_test.cpp imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_win32.o imgui_impl_dx11.o buffer.o hook.o trampoline.o hde64.o -o overlay_hook_test.exe -ld3d11 -ldxgi -lxinput -ld3dcompiler -lgdi32 -ldwmapi
if errorlevel 1 exit /b 1
overlay_hook_test.exe
exit /b %errorlevel%
