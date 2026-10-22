# 仅在用户授权编译后执行；输入目录必须先由 package_delivery.py 生成。
param([Parameter(Mandatory = $true)][string]$InputDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskInput = (Resolve-Path -LiteralPath $InputDirectory).Path
$taskReportPath = Join-Path $taskInput '封装回执.json'
$taskReport = Get-Content -LiteralPath $taskReportPath -Raw -Encoding utf8 | ConvertFrom-Json
if ($taskReport.exe_compiled) { throw '本目录已记录 EXE 编译，拒绝覆盖。请生成新的封装目录。' }
foreach ($entry in $taskReport.installer_sources_sha256.PSObject.Properties) {
    if ($entry.Name -notmatch '^[a-zA-Z0-9_.-]+$') { throw '源码文件名无效。' }
    $taskSource = Join-Path (Join-Path $taskInput 'setup-source') $entry.Name
    if ((Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) {
        throw "冻结源码已改变：$($entry.Name)"
    }
}
if ($taskReport.exe_filename -notmatch '^AC7MultiSP-[0-9.]+-Setup-preview-[0-9.]+\.exe$') { throw '成品名称无效。' }
$taskExe = Join-Path $taskInput $taskReport.exe_filename
if (Test-Path -LiteralPath $taskExe) { throw 'EXE 已存在，拒绝覆盖。' }
$taskCompiler = Join-Path ([Environment]::GetFolderPath('Windows')) 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if (-not (Test-Path -LiteralPath $taskCompiler -PathType Leaf)) { throw '未找到系统 .NET Framework 64位编译器，请报告环境，不自动安装工具。' }
& $taskCompiler ('@' + (Join-Path $taskInput 'setup-build.rsp'))
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $taskExe -PathType Leaf)) { throw 'EXE 编译未成功，请保留输出；不自动重试。' }
$taskResult = [ordered]@{
    schema = 1
    exe = $taskExe
    sha256 = (Get-FileHash -LiteralPath $taskExe -Algorithm SHA256).Hash.ToLowerInvariant()
    source_receipt_sha256 = (Get-FileHash -LiteralPath $taskReportPath -Algorithm SHA256).Hash.ToLowerInvariant()
    exe_compiled = $true
    installer_executed = $false
    tests_run = $false
    game_installed = $false
}
$taskResultPath = Join-Path $taskInput 'EXE编译回执.json'
if (Test-Path -LiteralPath $taskResultPath) { throw '编译回执已存在，拒绝覆盖。' }
$taskResult | ConvertTo-Json | Set-Content -LiteralPath $taskResultPath -Encoding utf8
$taskResult | ConvertTo-Json
