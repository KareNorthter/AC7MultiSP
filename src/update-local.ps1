$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$gameRoot='E:\SteamLibrary\steamapps\common\ACE COMBAT 7'
$modRoot=Join-Path $gameRoot 'ue4ss\Mods\DualSP'
if(Get-Process Ace7Game -ErrorAction SilentlyContinue){throw '游戏正在运行，保留更新包，等待用户退出后安装。'}
$profileHash=(Get-FileHash -LiteralPath (Join-Path $modRoot 'profiles.json')).Hash
# 存档位置跟随当前 Windows 用户，不绑定旧用户名。
$saveRoot=Join-Path $env:LOCALAPPDATA 'BANDAI NAMCO Entertainment\ACE COMBAT 7'
$protectedFiles=@(Get-ChildItem -LiteralPath $saveRoot -Recurse -File | Where-Object {$_.Extension -eq '.sav' -or $_.Name -eq 'GraphicsSetting.ini'})
$before=@{}
foreach($file in $protectedFiles){$before[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}
$exeHash=(Get-FileHash -LiteralPath (Join-Path $gameRoot 'Ace7Game.exe')).Hash
& (Join-Path $projectRoot 'Install.ps1') -GamePath $gameRoot
$checks=@()
foreach($relative in @('bin\DualSP.dll','bin\DualSPUI.dll','Scripts\main.lua','Scripts\json.lua','Scripts\weapon_cycle.lua','Scripts\native_hud.lua','Scripts\loadout_flow.lua','Scripts\native_loadout.lua')){
    $installedHash=(Get-FileHash -LiteralPath (Join-Path $modRoot $relative)).Hash
    $expectedHash=(Get-FileHash -LiteralPath (Join-Path $projectRoot ('dist\DualSP\'+$relative))).Hash
    if($installedHash -ne $expectedHash){throw "更新文件不一致：$relative"}
    $checks+=[PSCustomObject]@{File=$relative;SHA256=$installedHash;Matches=$true}
}
$profilesPreserved=$profileHash -eq (Get-FileHash -LiteralPath (Join-Path $modRoot 'profiles.json')).Hash
$savesPreserved=$true
foreach($file in $protectedFiles){if((Get-FileHash -LiteralPath $file.FullName).Hash -ne $before[$file.FullName]){$savesPreserved=$false}}
$exePreserved=$exeHash -eq (Get-FileHash -LiteralPath (Join-Path $gameRoot 'Ace7Game.exe')).Hash
if(-not($profilesPreserved -and $savesPreserved -and $exePreserved)){throw '更新核验发现原配置或游戏文件变化，查看备份。'}
$result=[PSCustomObject]@{
    Version=(Get-Content -LiteralPath (Join-Path $modRoot 'install-receipt.json') -Raw | ConvertFrom-Json).Version
    ProfilesPreserved=$profilesPreserved;SaveAndGraphicsFilesPreserved=$savesPreserved
    ExecutablePreserved=$exePreserved;Files=$checks;GameStartedByAgent=$false
}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $projectRoot 'evidence\0.1.7-installed-verification.json') -Encoding utf8
$result | ConvertTo-Json -Depth 5
