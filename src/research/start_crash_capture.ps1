# 有限跟踪游戏启动交接，自动采集异常、无响应和退出现场；不启动或终止游戏。
param([string]$WorkerOutput = '') # 内部工作模式：有限跟踪启动器到实际游戏的进程交接。
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) # 当前研究工作区。
$captureTool = Join-Path $taskRoot 'Temp/crash-capture-tools/ProcDump/procdump64.exe' # 微软签名工具。
if ($WorkerOutput) {
    $deadline = (Get-Date).AddHours(2) # 只等待本轮测试，不注册常驻任务。
    $sessionCount = 0 # 最多跟踪四个进程，包含启动交接。
    $statusFile = Join-Path $WorkerOutput 'status.json' # 当前实际附加 PID 与状态。
    try {
        while ($sessionCount -lt 4 -and (Get-Date) -lt $deadline) {
            @{ state='waiting'; sessions=$sessionCount; deadline=$deadline.ToString('o') } | ConvertTo-Json | Set-Content -LiteralPath $statusFile -Encoding utf8
            $targetProcess = $null # 不复用已退出进程的 PID。
            while (-not $targetProcess -and (Get-Date) -lt $deadline) {
                $targetProcess = Get-Process -Name Ace7Game -ErrorAction SilentlyContinue | Select-Object -First 1
                if (-not $targetProcess) { Start-Sleep -Seconds 1 }
            }
            if (-not $targetProcess) { break }
            $sessionCount++
            $sessionDir = Join-Path $WorkerOutput ('session-{0}-pid-{1}' -f $sessionCount,$targetProcess.Id) # 各次记录隔离。
            New-Item -ItemType Directory -Path $sessionDir | Out-Null
            $captureArgs = @('-accepteula','-mm','-e','1','-f','C0000005,C0000409,C0000374,C00000FD,C000001D',
                '-h','-t','-n','6','-s','1',$targetProcess.Id,('"'+$sessionDir+'"')) # 最多六份转储。
            $monitor = Start-Process -FilePath $captureTool -ArgumentList $captureArgs -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $sessionDir 'procdump.stdout.log') `
                -RedirectStandardError (Join-Path $sessionDir 'procdump.stderr.log')
            @{ state='monitoring'; game_pid=$targetProcess.Id; monitor_pid=$monitor.Id; session=$sessionCount } | ConvertTo-Json | Set-Content -LiteralPath $statusFile -Encoding utf8
            $monitor.WaitForExit() # 游戏结束后继续等待，启动引导进程退出不结束整轮。
            $targetProcess.Refresh()
            if (-not $targetProcess.HasExited) {
                throw '采集器已结束但目标仍在运行；可能达到转储上限或附加失败，停止自动重挂，请检查日志。'
            }
            $modDir = 'E:\SteamLibrary\steamapps\common\ACE COMBAT 7\ue4ss\Mods\DualSP' # 进程退出后保留配套记录。
            foreach ($logName in @('mod.log','native.log','ui-state.json')) {
                Copy-Item -LiteralPath (Join-Path $modDir $logName) -Destination $sessionDir
            }
        }
        @{ state='finished'; sessions=$sessionCount; reason='达到四个进程或等待期限' } | ConvertTo-Json | Set-Content -LiteralPath $statusFile -Encoding utf8
    } catch {
        @{ state='error'; error=$_.Exception.Message; sessions=$sessionCount } | ConvertTo-Json | Set-Content -LiteralPath $statusFile -Encoding utf8
        throw
    }
    exit
}
$signature = Get-AuthenticodeSignature -LiteralPath $captureTool # 启动前验证来源。
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
    throw 'ProcDump 的微软签名无效，停止启动。'
}
$existing = @(Get-Process -Name procdump,procdump64,procdump64a -ErrorAction SilentlyContinue) # 避免多个调试器争用游戏。
if ($existing.Count -gt 0) { throw '已有 ProcDump 运行，请核对现有采集会话，不能重复启动。' }
$workers = @(Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object { $_.ProcessId -ne $PID -and $_.CommandLine -like '*start_crash_capture.ps1*' -and $_.CommandLine -like '*-WorkerOutput*' }) # 等待中的采集也不能重复。
if ($workers.Count -gt 0) { throw '已有采集工作进程等待游戏，不能重复启动。' }
$captureDir = Join-Path $taskRoot ('Temp/crash-capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) # 每次独立保存。
New-Item -ItemType Directory -Path $captureDir | Out-Null
$captureArgs = @('-NoProfile','-NonInteractive','-File',('"'+$PSCommandPath+'"'),'-WorkerOutput',('"'+$captureDir+'"')) # 隐藏工作进程等待有限批次。
$captureProcess = Start-Process -FilePath 'C:\Program Files\PowerShell\7\pwsh.exe' -ArgumentList $captureArgs -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $captureDir 'worker.stdout.log') `
    -RedirectStandardError (Join-Path $captureDir 'worker.stderr.log')
$receipt = [ordered]@{ # 只保存工具与启动信息，不将启动成功冒充捕获成功。
    started_at = [DateTimeOffset]::Now.ToString('o'); worker_pid = $captureProcess.Id
    tool = $captureTool; tool_sha256 = (Get-FileHash -LiteralPath $captureTool -Algorithm SHA256).Hash
    arguments = $captureArgs; output = $captureDir; game_started = $false; captured_verified = $false
    source = 'https://learn.microsoft.com/en-us/sysinternals/downloads/procdump'
}
$receipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $captureDir 'launch.json') -Encoding utf8
Start-Sleep -Milliseconds 700
$captureProcess.Refresh()
if ($captureProcess.HasExited) { throw "采集工具已退出，查看 $captureDir 中的日志。" }
$receipt | ConvertTo-Json -Depth 4
