param([string]$GamePath = 'E:\SteamLibrary\steamapps\common\ACE COMBAT 7')
$ErrorActionPreference='Stop'
$GamePath=(Resolve-Path -LiteralPath $GamePath).ProviderPath
if(Get-Process Ace7Game -ErrorAction SilentlyContinue){throw '请先退出游戏，再卸载模组。'}
$loader=Join-Path $GamePath 'ue4ss'
$mod=Join-Path $loader 'Mods\DualSP'
if(-not(Test-Path -LiteralPath $mod)){Write-Output '游戏目录中未安装 DualSP。';exit}
$resolved=(Resolve-Path -LiteralPath $mod).ProviderPath
if(-not $resolved.StartsWith($GamePath+'\',[StringComparison]::OrdinalIgnoreCase)){throw '模组路径超出游戏目录。'}
$receiptFile=Join-Path $mod 'install-receipt.json'
$receipt=if(Test-Path -LiteralPath $receiptFile){Get-Content -LiteralPath $receiptFile -Raw|ConvertFrom-Json}else{$null}
$backup=Join-Path $GamePath ('DualSP-Backups\uninstall-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$backup=[IO.Path]::GetFullPath($backup)
if(-not $backup.StartsWith($GamePath+'\',[StringComparison]::OrdinalIgnoreCase)){throw '备份路径超出游戏目录。'}
New-Item -ItemType Directory -Path $backup -Force|Out-Null
Move-Item -LiteralPath $resolved -Destination (Join-Path $backup 'DualSP')
$list=Join-Path $loader 'Mods\mods.txt'
$text=if(Test-Path -LiteralPath $list){[IO.File]::ReadAllText($list)}else{''}
if(Test-Path -LiteralPath $list){Copy-Item -LiteralPath $list -Destination (Join-Path $backup 'mods.txt')}
$text=[regex]::Replace($text,'(?m)^\s*DualSP\s*:\s*[01]\s*$','').Trim()
[IO.File]::WriteAllText($list,$text+"`n",[Text.UTF8Encoding]::new($false))
$otherMods=@(Get-ChildItem -LiteralPath (Join-Path $loader 'Mods') -Directory)
if($receipt -and $receipt.OwnsLoader -and $otherMods.Count -eq 0){
    $resolvedLoader=(Resolve-Path -LiteralPath $loader).ProviderPath
    if(-not $resolvedLoader.StartsWith($GamePath+'\',[StringComparison]::OrdinalIgnoreCase)){throw '加载器路径超出游戏目录。'}
    Move-Item -LiteralPath $resolvedLoader -Destination (Join-Path $backup 'ue4ss')
    $proxy=Join-Path $GamePath 'dwmapi.dll'
    if(Test-Path -LiteralPath $proxy){Move-Item -LiteralPath $proxy -Destination (Join-Path $backup 'dwmapi.dll')}
}
Write-Output "已卸载 DualSP。配置和文件保留在：$backup"
