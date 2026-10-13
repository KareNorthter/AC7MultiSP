$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$payload=Join-Path $root 'payload\ue4ss\Mods\DualSP'
$core=Join-Path $root 'dist\DualSP\bin\DualSP.dll'
if((Get-FileHash -LiteralPath $core).Hash -ne 'D7D7055CA7909021EEACA15E63FF5FF253D2EB7E60A3FE3B79DCEEAB231DE339'){throw 'Tested core was changed; stop release preparation.'}
# Git 不跟踪重复载荷，首次从版本库恢复时先创建同步目标目录。
New-Item -ItemType Directory -Path (Join-Path $payload 'bin'),(Join-Path $payload 'Scripts') -Force|Out-Null
foreach($relative in @('bin\DualSP.dll','bin\DualSPUI.dll','Scripts\main.lua','Scripts\json.lua','Scripts\weapon_cycle.lua','Scripts\native_hud.lua','Scripts\loadout_flow.lua','Scripts\native_loadout.lua')){
    $source=Join-Path $root ('dist\DualSP\'+$relative)
    $target=Join-Path $payload $relative
    Copy-Item -LiteralPath $source -Destination $target
    if((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash){throw "Payload mismatch: $relative"}
}
foreach($path in @((Join-Path $root 'Install.ps1'),(Join-Path $PSScriptRoot 'package.ps1'),(Join-Path $PSScriptRoot 'update-local.ps1'))){
    $content=[IO.File]::ReadAllText($path)
    [IO.File]::WriteAllText($path,$content,[Text.UTF8Encoding]::new($true))
}
