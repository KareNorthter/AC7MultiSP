@echo off
setlocal
rem 独立生成核心候选，保留已由用户实测的正式核心。
if not defined DUALSP_TOOLCHAIN set "DUALSP_TOOLCHAIN=%~dp0..\tools\llvm-mingw-20261006-ucrt-x86_64"
set "CXX=%DUALSP_TOOLCHAIN%\bin\clang++.exe"
set "CC=%DUALSP_TOOLCHAIN%\bin\clang.exe"
if not exist "%CXX%" (echo LLVM-MinGW compiler missing. & exit /b 1)
cd /d "%~dp0.."
if not exist build-core mkdir build-core
if not exist Temp\compiler mkdir Temp\compiler
set "TMP=%CD%\Temp\compiler"
set "TEMP=%TMP%"
rem 补给页输入钩子使用项目已有的 MinHook，单独生成核心对象文件。
cd build-core
"%CC%" -O2 -c -I ../vendor/minhook-1.3.4/include ../vendor/minhook-1.3.4/src/buffer.c ../vendor/minhook-1.3.4/src/hook.c ../vendor/minhook-1.3.4/src/trampoline.c ../vendor/minhook-1.3.4/src/hde/hde64.c
if errorlevel 1 exit /b 1
"%CXX%" -std=c++17 -O2 -shared -static -I ../vendor -I ../vendor/minhook-1.3.4/include ../src/weapon_bridge.cpp buffer.o hook.o trampoline.o hde64.o -o DualSP.dll
exit /b %errorlevel%
