@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
echo AC7MultiSP 2.22.0 候选安装
echo 请先正常退出游戏。本脚本只安装已编译并通过11项检查的成套产物。
echo 会核对2.21.1基线、备份存档和Mod目录，并保留个人配置及其他Mod。
"C:\Users\83717\AppData\Local\Programs\Python\Python313\python.exe" -X utf8 src\install.py --build-dir Temp/build-2.22.0-candidate-r2 --base-receipt evidence/2.21.1-installed.json --apply
if errorlevel 1 goto failed
echo 安装与核验完成。回执：evidence\2.22.0-installed.json
echo 请从Steam正常启动游戏，测试新镜头功能及四架候选机体。
pause
exit /b 0
:failed
echo 安装未完成，请保留上方错误信息；不要手动覆盖文件或删除备份。
pause
exit /b 1
