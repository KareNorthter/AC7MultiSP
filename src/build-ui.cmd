@echo off
setlocal
rem Compatibility entry: always build the complete product into Temp.
python -X utf8 "%~dp0build.py" %*
exit /b %errorlevel%
