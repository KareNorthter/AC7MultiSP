@echo off
setlocal
rem 独立生成核心候选，保留已由用户实测的正式核心。
if not defined DUALSP_TOOLCHAIN set "DUALSP_TOOLCHAIN=%~dp0..\tools\llvm-mingw-20261006-ucrt-x86_64"
set "CXX=%DUALSP_TOOLCHAIN%\bin\clang++.exe"
if not exist "%CXX%" (echo LLVM-MinGW compiler missing. & exit /b 1)
cd /d "%~dp0.."
if not exist build-core mkdir build-core
if not exist Temp\compiler mkdir Temp\compiler
set "TMP=%CD%\Temp\compiler"
set "TEMP=%TMP%"
"%CXX%" -std=c++17 -O2 -shared -static -I vendor src/weapon_bridge.cpp -o build-core/DualSP.dll
exit /b %errorlevel%
