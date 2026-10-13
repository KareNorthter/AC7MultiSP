param([string]$GamePath = 'E:\SteamLibrary\steamapps\common\ACE COMBAT 7')
$ErrorActionPreference='Stop'
$GamePath=(Resolve-Path -LiteralPath $GamePath).ProviderPath
$exe=Join-Path $GamePath 'Ace7Game.exe'
$expected='C7DA97F5F8A807D4F1264ADBB074146FCFFE9BDC2FFA98791B822CD28E558F4F'
if(-not(Test-Path -LiteralPath $exe) -or (Get-FileHash -LiteralPath $exe).Hash -ne $expected){throw '当前安装包仅支持已验证的 Steam build 9855922；游戏版本不匹配。'}
if(Get-Process Ace7Game -ErrorAction SilentlyContinue){throw '请先退出皇牌空战 7，再运行安装。'}
$payload=Join-Path $PSScriptRoot 'payload'
$loader=Join-Path $GamePath 'ue4ss'
$proxy=Join-Path $GamePath 'dwmapi.dll'
$ownsLoader=$false
$backup=Join-Path $GamePath ('DualSP-Backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$backup=[IO.Path]::GetFullPath($backup)
if(-not $backup.StartsWith($GamePath+'\',[StringComparison]::OrdinalIgnoreCase)){throw '备份路径超出游戏目录。'}
if((Test-Path -LiteralPath $loader) -or (Test-Path -LiteralPath $proxy)){
    if(-not(Test-Path -LiteralPath (Join-Path $loader 'UE4SS.dll')) -or -not(Test-Path -LiteralPath $proxy)){throw '检测到不完整的加载器，请先处理原有安装。'}
    foreach($pair in @(@($proxy,(Join-Path $payload 'dwmapi.dll')),@((Join-Path $loader 'UE4SS.dll'),(Join-Path $payload 'ue4ss\UE4SS.dll')))){
        if((Get-FileHash -LiteralPath $pair[0]).Hash -ne (Get-FileHash -LiteralPath $pair[1]).Hash){throw '已有加载器与本模组要求的 AC7 专用 UE4SS 不同；为避免覆盖，安装已停止。'}
    }
}else{
    New-Item -ItemType Directory -Path $loader,(Join-Path $loader 'Mods') -Force|Out-Null
    Copy-Item -LiteralPath (Join-Path $payload 'dwmapi.dll') -Destination $GamePath
    Copy-Item -LiteralPath (Join-Path $payload 'ue4ss\UE4SS.dll'),(Join-Path $payload 'ue4ss\UE4SS-settings.ini') -Destination $loader
    $ownsLoader=$true
}
$mod=Join-Path $loader 'Mods\DualSP'
if(Test-Path -LiteralPath $mod){
    $resolved=(Resolve-Path -LiteralPath $mod).ProviderPath
    if(-not $resolved.StartsWith($GamePath+'\',[StringComparison]::OrdinalIgnoreCase)){throw '模组路径超出游戏目录。'}
    $previousReceipt=Join-Path $mod 'install-receipt.json'
    if(Test-Path -LiteralPath $previousReceipt){$ownsLoader=$ownsLoader -or (Get-Content -LiteralPath $previousReceipt -Raw|ConvertFrom-Json).OwnsLoader}
    New-Item -ItemType Directory -Path $backup -Force|Out-Null
    Move-Item -LiteralPath $resolved -Destination (Join-Path $backup 'DualSP')
}
Copy-Item -LiteralPath (Join-Path $payload 'ue4ss\Mods\DualSP') -Destination (Join-Path $loader 'Mods') -Recurse
$previousProfiles=Join-Path $backup 'DualSP\profiles.json'
if(Test-Path -LiteralPath $previousProfiles){Copy-Item -LiteralPath $previousProfiles -Destination $mod}
$list=Join-Path $loader 'Mods\mods.txt'
$text=if(Test-Path -LiteralPath $list){[IO.File]::ReadAllText($list)}else{''}
if(Test-Path -LiteralPath $list){New-Item -ItemType Directory -Path $backup -Force|Out-Null;Copy-Item -LiteralPath $list -Destination (Join-Path $backup 'mods.txt')}
$text=[regex]::Replace($text,'(?m)^\s*DualSP\s*:\s*[01]\s*$','').TrimEnd()+"`nDualSP : 1`n"
[IO.File]::WriteAllText($list,$text,[Text.UTF8Encoding]::new($false))
$receipt=[PSCustomObject]@{Version='0.1.7';OwnsLoader=$ownsLoader;GamePath=$GamePath;InstalledAt=(Get-Date -Format o)}
$receipt|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $mod 'install-receipt.json') -Encoding utf8
foreach($relative in @('bin\DualSP.dll','bin\DualSPUI.dll','Scripts\main.lua','Scripts\json.lua','Scripts\weapon_cycle.lua','Scripts\native_hud.lua','Scripts\loadout_flow.lua','Scripts\native_loadout.lua')){
    $installed=Join-Path $mod $relative
    $shipped=Join-Path (Join-Path $payload 'ue4ss\Mods\DualSP') $relative
    if(-not(Test-Path -LiteralPath $installed) -or (Get-FileHash -LiteralPath $installed).Hash -ne (Get-FileHash -LiteralPath $shipped).Hash){throw "安装后文件核验失败：$relative"}
}
Write-Output 'DualSP 0.1.7 已安装。原兵装页选择 SP1 / SP2，再由原生确认继续；F8 保留为备用和调试入口。'
