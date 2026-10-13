# 只在用户明确要求发布时执行；不编译、不运行检查，不覆盖已有快照。
$ErrorActionPreference='Stop'

# 发布路径统一由项目根目录和版本文件确定。
$root=Split-Path $PSScriptRoot -Parent
$version=[IO.File]::ReadAllText((Join-Path $root 'VERSION')).Trim()
$release=Join-Path $root ("Release\DualSP-$version")
$source=Join-Path $root ("Release\DualSP-$version-source")
$releaseZip=Join-Path $root ("Release\DualSP-$version.zip")
$sourceZip=Join-Path $root ("Release\DualSP-$version-source.zip")
foreach($path in @($release,$source,$releaseZip,$sourceZip)){
    if(Test-Path -LiteralPath $path){throw "已有发布资产，不覆盖：$path"}
}
New-Item -ItemType Directory -Path $release,$source -Force|Out-Null

# 安装包包含统一文档入口和完整安装载荷。
foreach($name in @('Install.ps1','Uninstall.ps1','Install.cmd','Uninstall.cmd','README.md','AGENTS.md','VERSION','docs','payload')){
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $release -Recurse
}

# 源码包复制当前资产，不夹带工具、个人备份和历史版本。
foreach($name in @('AGENTS.md','README.md','VERSION','.gitignore','.tfignore','.gitattributes',
    'Install.ps1','Uninstall.ps1','Install.cmd','Uninstall.cmd','src','vendor','dist','payload','tests','docs','evidence','references')){
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $source -Recurse
}
New-Item -ItemType Directory -Path ($source+'\tools') -Force|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'tools\README.md') -Destination ($source+'\tools')

# 安装包清单用于核对分发内容；压缩包固定放在 Release 中。
$manifest=Get-ChildItem -LiteralPath $release -Recurse -File|ForEach-Object {
    [PSCustomObject]@{Path=[IO.Path]::GetRelativePath($release,$_.FullName);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
}
$manifest|ConvertTo-Json -Depth 4|Set-Content -LiteralPath ($release+'\manifest.json') -Encoding utf8
Compress-Archive -LiteralPath $release -DestinationPath $releaseZip
Compress-Archive -LiteralPath $source -DestinationPath $sourceZip
Write-Output "发布资产已写入：$releaseZip 与 $sourceZip"
