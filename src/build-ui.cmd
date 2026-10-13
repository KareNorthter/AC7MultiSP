@echo off
setlocal
if not defined DUALSP_TOOLCHAIN set "DUALSP_TOOLCHAIN=%~dp0..\tools\llvm-mingw-20261006-ucrt-x86_64"
set "CXX=%DUALSP_TOOLCHAIN%\bin\clang++.exe"
set "CC=%DUALSP_TOOLCHAIN%\bin\clang.exe"
if not exist "%CXX%" (echo Set DUALSP_TOOLCHAIN to the LLVM-MinGW UCRT x64 folder. & exit /b 1)
cd /d "%~dp0.."
if not exist dist\DualSP\bin mkdir dist\DualSP\bin
if not exist build-ui mkdir build-ui
cd build-ui
if not exist ..\Temp\compiler mkdir ..\Temp\compiler
set "TMP=%CD%\..\Temp\compiler"
set "TEMP=%TMP%"
"%CC%" -O2 -c -I ../vendor/minhook-1.3.4/include ../vendor/minhook-1.3.4/src/buffer.c ../vendor/minhook-1.3.4/src/hook.c ../vendor/minhook-1.3.4/src/trampoline.c ../vendor/minhook-1.3.4/src/hde/hde64.c
if errorlevel 1 exit /b 1
"%CXX%" -std=c++17 -O2 -c -I ../vendor -I ../vendor/minhook-1.3.4/include -I ../vendor/imgui-1.92.9b -I ../vendor/imgui-1.92.9b/backends ../src/overlay.cpp ../vendor/imgui-1.92.9b/imgui.cpp ../vendor/imgui-1.92.9b/imgui_draw.cpp ../vendor/imgui-1.92.9b/imgui_tables.cpp ../vendor/imgui-1.92.9b/imgui_widgets.cpp ../vendor/imgui-1.92.9b/backends/imgui_impl_win32.cpp ../vendor/imgui-1.92.9b/backends/imgui_impl_dx11.cpp
if errorlevel 1 exit /b 1
"%CXX%" -shared -static overlay.o imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_win32.o imgui_impl_dx11.o buffer.o hook.o trampoline.o hde64.o -o ../dist/DualSP/bin/DualSPUI.dll -ld3d11 -ldxgi -lxinput -ld3dcompiler -lgdi32 -ldwmapi
exit /b %errorlevel%
