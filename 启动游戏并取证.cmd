@echo off
"C:\Program Files\PowerShell\7\pwsh.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0src\research\start_crash_capture.ps1"
if errorlevel 1 pause
