# 提前等待一次游戏运行，自动采集异常、无响应和退出现场；不启动或终止游戏。
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) # 当前研究工作区。
$captureTool = Join-Path $taskRoot 'Temp/crash-capture-tools/ProcDump/procdump64.exe' # 微软签名工具。
$signature = Get-AuthenticodeSignature -LiteralPath $captureTool # 启动前验证来源。
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
    throw 'ProcDump 的微软签名无效，停止启动。'
}
$existing = @(Get-Process -Name procdump,procdump64,procdump64a -ErrorAction SilentlyContinue) # 避免多个调试器争用游戏。
if ($existing.Count -gt 0) { throw '已有 ProcDump 运行，请核对现有采集会话，不能重复启动。' }
$captureDir = Join-Path $taskRoot ('Temp/crash-capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) # 每次独立保存。
New-Item -ItemType Directory -Path $captureDir | Out-Null
$captureArgs = @('-accepteula', '-mm', '-e', '1', '-f', 'C0000005,C0000409,C0000374,C00000FD,C000001D',
    '-h', '-t', '-n', '6', '-s', '1', '-w', 'Ace7Game.exe', ('"' + $captureDir + '"')) # 过滤常见致命异常；最多六份。
$captureProcess = Start-Process -FilePath $captureTool -ArgumentList $captureArgs -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $captureDir 'procdump.stdout.log') `
    -RedirectStandardError (Join-Path $captureDir 'procdump.stderr.log')
$receipt = [ordered]@{ # 只保存工具与启动信息，不将启动成功冒充捕获成功。
    started_at = [DateTimeOffset]::Now.ToString('o'); monitor_pid = $captureProcess.Id
    tool = $captureTool; tool_sha256 = (Get-FileHash -LiteralPath $captureTool -Algorithm SHA256).Hash
    arguments = $captureArgs; output = $captureDir; game_started = $false; captured_verified = $false
    source = 'https://learn.microsoft.com/en-us/sysinternals/downloads/procdump'
}
$receipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $captureDir 'launch.json') -Encoding utf8
Start-Sleep -Milliseconds 700
$captureProcess.Refresh()
if ($captureProcess.HasExited) { throw "采集工具已退出，查看 $captureDir 中的日志。" }
$receipt | ConvertTo-Json -Depth 4
