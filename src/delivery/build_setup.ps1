# 仅在用户授权编译后执行；输入目录必须先由 package_delivery.py 生成。
param([Parameter(Mandatory = $true)][string]$InputDirectory,
      [string]$SigningKeyName,
      [string]$SigningProvider = 'Microsoft Software Key Storage Provider',
      [string]$Python = 'python')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskInput = (Resolve-Path -LiteralPath $InputDirectory).Path
$taskReportPath = Join-Path $taskInput '封装回执.json'
$taskReport = Get-Content -LiteralPath $taskReportPath -Raw -Encoding utf8 | ConvertFrom-Json
if (-not $taskReport.author_signing) { throw '新安装器必须有作者签名，不接受无签名输入。' }
$taskSigningTool = Join-Path $PSScriptRoot '../author_signing.py'
$taskTrust = Join-Path $PSScriptRoot 'trusted-author-keys.json'
function Assert-SigningSources {
    foreach ($taskSigningSource in @(@($taskSigningTool, 'src/author_signing.py'), @($taskTrust, 'src/delivery/trusted-author-keys.json'))) {
        if ((Get-FileHash -LiteralPath $taskSigningSource[0] -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskReport.source_files.($taskSigningSource[1])) {
            throw '签名工具或公钥与封装来源不同。'
        }
    }
}
Assert-SigningSources
& $Python -X utf8 $taskSigningTool verify-inputs --files-dir $taskInput
if ($LASTEXITCODE -ne 0) { throw '作者签名及冻结资源核验未完成，不编译。' }
if ($taskReport.exe_compiled) { throw '本目录已记录 EXE 编译，拒绝覆盖。请生成新的封装目录。' }
foreach ($entry in $taskReport.installer_sources_sha256.PSObject.Properties) {
    if ($entry.Name -notmatch '^[a-zA-Z0-9_.-]+$') { throw '源码文件名无效。' }
    $taskSource = Join-Path (Join-Path $taskInput 'setup-source') $entry.Name
    if ((Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) {
        throw "冻结源码已改变：$($entry.Name)"
    }
}
function Assert-InstallerInputs {
    foreach ($taskEntry in $taskReport.installer_inputs_sha256.PSObject.Properties) {
        $taskPath = [IO.Path]::GetFullPath((Join-Path $taskInput $taskEntry.Name))
        if (-not $taskPath.StartsWith($taskInput + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw '编译输入路径越界。' }
        if (-not (Test-Path -LiteralPath $taskPath -PathType Leaf) -or
            (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskEntry.Value) {
            throw "冻结编译输入已改变：$($taskEntry.Name)"
        }
    }
    $taskZip = Join-Path $taskInput $taskReport.universal_zip
    if ((Get-FileHash -LiteralPath $taskZip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskReport.universal_sha256) { throw '独立通用ZIP已改变。' }
}
if (-not $taskReport.installer_inputs_sha256 -or @($taskReport.installer_inputs_sha256.PSObject.Properties).Count -lt 3) { throw '缺少完整编译输入摘要。' }
Assert-InstallerInputs
if ($taskReport.exe_filename -ne 'AC7MultiSP_Installer.exe') { throw '成品名称无效。' }
$taskExe = Join-Path $taskInput $taskReport.exe_filename
if (Test-Path -LiteralPath $taskExe) { throw 'EXE 已存在，拒绝覆盖。' }
$taskCompiler = Join-Path ([Environment]::GetFolderPath('Windows')) 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if (-not (Test-Path -LiteralPath $taskCompiler -PathType Leaf)) { throw '未找到系统 .NET Framework 64位编译器，请报告环境，不自动安装工具。' }
& $taskCompiler ('@' + (Join-Path $taskInput 'setup-build.rsp'))
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $taskExe -PathType Leaf)) { throw 'EXE 编译未成功，请保留输出；不自动重试。' }
Assert-InstallerInputs
$taskResult = [ordered]@{
    schema = 1
    exe = $taskExe
    sha256 = (Get-FileHash -LiteralPath $taskExe -Algorithm SHA256).Hash.ToLowerInvariant()
    source_receipt_sha256 = (Get-FileHash -LiteralPath $taskReportPath -Algorithm SHA256).Hash.ToLowerInvariant()
    exe_compiled = $true
    installer_executed = $false
    tests_run = $false
    game_installed = $false
    author_signing = $taskReport.author_signing
}
$taskResultPath = Join-Path $taskInput 'EXE编译回执.json'
if (Test-Path -LiteralPath $taskResultPath) { throw '编译回执已存在，拒绝覆盖。' }
$taskResult | ConvertTo-Json | Set-Content -LiteralPath $taskResultPath -Encoding utf8
Assert-SigningSources
$taskSealArguments = @('-X', 'utf8', $taskSigningTool, 'seal-release', '--files-dir', $taskInput, '--signing-provider', $SigningProvider)
if ($SigningKeyName) { $taskSealArguments += @('--signing-key-name', $SigningKeyName) }
& $Python @taskSealArguments
if ($LASTEXITCODE -ne 0) { throw 'EXE已编译，但最终发布签名未完成；本目录不可发布，请保留现场。' }
Assert-SigningSources
$taskResult | ConvertTo-Json
