# 每次启动游戏前运行：保存最初异常、退出调用栈及独立日志。
[CmdletBinding()]
param([switch]$WaitOnly, [switch]$CheckOnly,
    [string]$GameRoot = 'E:\SteamLibrary\steamapps\common\ACE COMBAT 7',
    [string]$WorkerOutput = '')
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$pwshPath = (Get-Command pwsh -ErrorAction Stop).Source
function Write-Json($Value, [string]$Path) {
    $Value | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding utf8
}
function Find-Debugger {
    $found = Get-Command cdb.exe -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }
    $package = Get-AppxPackage -Name Microsoft.WinDbg | Sort-Object Version -Descending | Select-Object -First 1
    if ($package) {
        $candidate = Join-Path $package.InstallLocation 'amd64/cdb.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw '未找到本机 WinDbg 的 cdb.exe，请先安装微软 WinDbg。'
}
function Start-Debugger([string[]]$Arguments) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $debugger
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = 'Hidden'
    $startInfo.RedirectStandardInput = $true
    foreach ($argument in $Arguments) { $startInfo.ArgumentList.Add($argument.Trim('"')) }
    return [Diagnostics.Process]::Start($startInfo)
}
# 不下载符号，不吞掉应用的异常处理；每个 PID 最多 8+2+2 份小转储。
function Write-DebugCommands([string]$Directory) {
    $dumpPath = $Directory.Replace('\','/')
    if ($dumpPath -match '[\s";]') { throw '取证目录不能包含空格、引号或分号，请使用简单项目路径。' }
    $commands = [Collections.Generic.List[string]]::new()
    $commands.Add('.symopt+ 0x100')
    $commands.Add('r @$t0=0; r @$t1=0; r @$t2=0')
    $second = '.echo CAPTURE_SECOND_CHANCE; .time; .exr -1; r; ~* kv; .if (@$t1 < 2) { r @$t1=@$t1+1; .dump /mipu /u ' + $dumpPath + '/unhandled.dmp }; gn'
    $commands.Add('sxd -c2 "' + $second + '" *')
    foreach ($code in @('c0000005','c0000409','c0000374','c00000fd','c000001d','40000015')) {
        $first = '.echo CAPTURE_FIRST_CHANCE; .time; .exr -1; r; kv; .if (@$t0 < 8) { r @$t0=@$t0+1; .dump /mipu /u ' + $dumpPath + '/first-chance.dmp }; gn'
        $commands.Add('sxe -c "' + $first + '" -c2 "' + $second + '" ' + $code)
    }
    foreach ($exitFunction in @('RtlExitUserProcess','NtTerminateProcess')) {
        $onExit = '.echo CAPTURE_EXIT_REQUEST_' + $exitFunction + '; .time; r; ~* kv; .if (@$t2 < 2) { r @$t2=@$t2+1; .dump /mipu /u ' + $dumpPath + '/exit-request.dmp }; gc'
        $commands.Add('bu ntdll!' + $exitFunction + ' "' + $onExit + '"')
    }
    # F12 可触发系统调试热键，必须自动放行，不能把无人值守采集变成全进程暂停。
    $commands.Add('sxe -c ".echo CAPTURE_BREAKPOINT_AUTO_CONTINUE; .time; kv; gh" bpe')
    $commands.Add('sxn out')
    $commands.Add('.echo CAPTURE_READY')
    $commands.Add('g')
    $commandFile = Join-Path $Directory 'capture.cdb'
    $commands | Set-Content -LiteralPath $commandFile -Encoding ascii
    return $commandFile
}
# 共享读取已打开的日志，周期快照只存末尾，退出时保留完整文件。
function Save-Logs([string]$Directory, [switch]$Full) {
    [IO.Directory]::CreateDirectory($Directory) | Out-Null
    $modRoot = Join-Path $GameRoot 'ue4ss/Mods/DualSP'
    $sources = [ordered]@{
        'UE4SS.log' = Join-Path $GameRoot 'ue4ss/UE4SS.log'
        'mod.log' = Join-Path $modRoot 'mod.log'
        'native.log' = Join-Path $modRoot 'native.log'
        'ui-state.json' = Join-Path $modRoot 'ui-state.json'
        'native-status.json' = Join-Path $modRoot 'native-status.json'
        'profiles.json' = Join-Path $modRoot 'profiles.json'
        'mods.txt' = Join-Path $GameRoot 'ue4ss/Mods/mods.txt'
        'VERSION' = Join-Path $modRoot 'VERSION'
        'install-receipt.json' = Join-Path $modRoot 'install-receipt.json'
    }
    $records = foreach ($entry in $sources.GetEnumerator()) {
        $sourceStream = $null; $destinationStream = $null
        try {
            if (-not (Test-Path -LiteralPath $entry.Value)) { continue }
            $sourceStream = [IO.File]::Open($entry.Value, 'Open', 'Read', [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
            $offset = if ($Full) { 0L } else { [Math]::Max(0L, $sourceStream.Length - 524288L) }
            $sourceStream.Position = $offset
            $destinationStream = [IO.File]::Create((Join-Path $Directory $entry.Key))
            $sourceStream.CopyTo($destinationStream)
            @{ file=$entry.Key; source=$entry.Value; offset=$offset; bytes=$destinationStream.Length }
        } catch { @{ file=$entry.Key; error=$_.Exception.Message } }
        finally {
            if ($destinationStream) { $destinationStream.Dispose() }
            if ($sourceStream) { $sourceStream.Dispose() }
        }
    }
    Write-Json @{ saved_at=[DateTimeOffset]::Now.ToString('o'); full=[bool]$Full; files=@($records) } (Join-Path $Directory 'snapshot.json')
}
$debugger = Find-Debugger
$signature = Get-AuthenticodeSignature -LiteralPath $debugger
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
    throw '调试器的微软签名无效，停止启动。'
}
if (-not (Test-Path -LiteralPath (Join-Path $GameRoot 'Ace7Game.exe'))) { throw "游戏目录无效：$GameRoot" }
if ($CheckOnly) {
    Write-Output "环境检查通过；调试器：$debugger；游戏：$GameRoot。未启动游戏或监听。"
    return
}
if (-not $WorkerOutput) {
    $existing = @(Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object {
        $_.ProcessId -ne $PID -and $_.CommandLine -like '*start_crash_capture.ps1*' -and $_.CommandLine -like '*-WorkerOutput*'
    })
    if ($existing.Count) {
        if ($existing.Count -eq 1 -and $existing[0].CommandLine -match '-WorkerOutput\s+"([^"]+)"') {
            $activeDir = $Matches[1]
            $active = Get-Content -LiteralPath (Join-Path $activeDir 'status.json') -Raw | ConvertFrom-Json
            if ($active.state -in @('waiting','attaching','monitoring')) {
                if (-not $WaitOnly -and -not (Get-Process -Name Ace7Game -ErrorAction SilentlyContinue)) {
                    Start-Process 'steam://rungameid/502500' | Out-Null
                }
                Write-Output "沿用已有取证进程，不重复附加。本轮目录：$activeDir"
                return
            }
        }
        throw '已有取证进程状态异常，请先查看 Temp 下最近的 crash-capture 目录。'
    }
    $captureDir = Join-Path $taskRoot ('Temp/crash-capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    [IO.Directory]::CreateDirectory($captureDir) | Out-Null
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-WorkerOutput',('"'+$captureDir+'"'),'-GameRoot',('"'+$GameRoot+'"'))
    $worker = Start-Process -FilePath $pwshPath -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $captureDir 'worker.stdout.log') `
        -RedirectStandardError (Join-Path $captureDir 'worker.stderr.log')
    Write-Json @{ started_at=[DateTimeOffset]::Now.ToString('o'); worker_pid=$worker.Id; debugger=$debugger;
        debugger_sha256=(Get-FileHash -LiteralPath $debugger).Hash; game_root=$GameRoot; wait_only=[bool]$WaitOnly;
        note='附加就绪以每个 PID 的 CAPTURE_READY 和 status.json 为准，正常退出也保留现场。' } (Join-Path $captureDir 'launch.json')
    $statusFile = Join-Path $captureDir 'status.json'
    $ready = $false
    for ($attempt=0; $attempt -lt 60; $attempt++) {
        if (Test-Path -LiteralPath $statusFile) {
            try { $state = Get-Content -LiteralPath $statusFile -Raw | ConvertFrom-Json } catch { $state = $null }
            if ($state.state -in @('waiting','attaching','monitoring')) { $ready = $true; break }
            if ($state.state -eq 'error') { throw $state.error }
        }
        $worker.Refresh()
        if ($worker.HasExited) { break }
        Start-Sleep -Milliseconds 250
    }
    if (-not $ready) { throw "监听未就绪，请查看 $captureDir。" }
    if (-not $WaitOnly -and -not (Get-Process -Name Ace7Game -ErrorAction SilentlyContinue)) {
        Start-Process 'steam://rungameid/502500' | Out-Null
    }
    Write-Output "取证已在后台等待/跟踪游戏。本轮目录：$captureDir"
    if (-not $WaitOnly) {
        Write-Output '正在等待游戏和取证器就绪……'
        $attachedDeadline = (Get-Date).AddSeconds(120)
        $confirmed = $false
        while ((Get-Date) -lt $attachedDeadline) {
            try { $state = Get-Content -LiteralPath $statusFile -Raw | ConvertFrom-Json } catch { $state = $null }
            if ($state.state -eq 'monitoring') { $confirmed = $true; break }
            if ($state.state -in @('error','finished')) { throw "取证未就绪：$($state.error)。请查看 $captureDir。" }
            Start-Sleep -Milliseconds 500
        }
        if (-not $confirmed) { throw "两分钟内未确认附加成功，后台仍按状态文件继续等待；请查看 $captureDir。" }
        Write-Output '取证已附加，可以正常游戏。此窗口即将关闭，后台继续记录。'
        Start-Sleep -Seconds 3
    }
    Write-Output '游戏退出后自动收尾；下次游戏请再次使用同一启动入口。'
    return
}
$statusFile = Join-Path $WorkerOutput 'status.json'
$seen = [Collections.Generic.HashSet[string]]::new()
$runStart = Get-Date
$waitUntil = $runStart.AddMinutes(10)
$sessionCount = 0
$monitor = $null
$mutex = [Threading.Mutex]::new($false, 'Local\AC7MultiSPCrashCapture')
$ownsMutex = $mutex.WaitOne(0)
try {
    if (-not $ownsMutex) { throw '已有取证工作进程运行。' }
    $inventory = foreach ($relative in @('ue4ss/UE4SS.dll','dwmapi.dll','ue4ss/Mods/DualSP','ue4ss/Mods/AC7MouseAim')) {
        $candidate = Join-Path $GameRoot $relative
        if (-not (Test-Path -LiteralPath $candidate)) { continue }
        Get-ChildItem -LiteralPath $candidate -File -Recurse | Where-Object { $_.Extension -in @('.dll','.lua','.ini') } | ForEach-Object {
            @{ path=$_.FullName; bytes=$_.Length; sha256=(Get-FileHash -LiteralPath $_.FullName).Hash }
        }
    }
    Write-Json @{ at=[DateTimeOffset]::Now.ToString('o'); files=@($inventory) } (Join-Path $WorkerOutput 'runtime-files.json')
    Write-Json @{ state='waiting'; worker_pid=$PID; until=$waitUntil.ToString('o') } $statusFile
    while ((Get-Date) -lt $waitUntil -and $sessionCount -lt 8) {
        $target = Get-Process -Name Ace7Game -ErrorAction SilentlyContinue | Where-Object {
            -not $seen.Contains(('{0}-{1}' -f $_.Id,$_.StartTime.Ticks))
        } | Sort-Object StartTime -Descending | Select-Object -First 1
        if (-not $target) { Start-Sleep -Milliseconds 200; continue }
        $seen.Add(('{0}-{1}' -f $target.Id,$target.StartTime.Ticks)) | Out-Null
        $sessionCount++
        $sessionDir = Join-Path $WorkerOutput ('session-{0}-pid-{1}' -f $sessionCount,$target.Id)
        [IO.Directory]::CreateDirectory($sessionDir) | Out-Null
        Save-Logs (Join-Path $sessionDir 'before')
        $commandFile = Write-DebugCommands $sessionDir
        $cdbLog = Join-Path $sessionDir 'debugger.log'
        $arguments = @('-pd','-G','-sins','-y',('"'+$sessionDir+'"'),'-logo',('"'+$cdbLog+'"'),'-cf',('"'+$commandFile+'"'),'-p',$target.Id)
        Write-Json @{ state='attaching'; worker_pid=$PID; game_pid=$target.Id; session=$sessionDir } $statusFile
        $monitor = Start-Debugger $arguments
        $attachStarted = Get-Date
        $attached = $false
        $nextSnapshot = Get-Date
        $knownDumps = [Collections.Generic.HashSet[string]]::new()
        while (-not $monitor.HasExited) {
            # 有意使用固定操作，避免以后需要临时复制控制管道句柄才能保存卡死现场。
            $controlFile = Join-Path $WorkerOutput 'control.txt'
            if (Test-Path -LiteralPath $controlFile) {
                $control = (Get-Content -LiteralPath $controlFile -Raw).Trim()
                $controlCommands = switch ($control) {
                    'snapshot' { '.echo MANUAL_SNAPSHOT; .time; .exr -1; ~* kv; .dump /mipu /u ' + $sessionDir.Replace('\','/') + '/manual.dmp' }
                    'continue-breakpoint' { 'sxe -c ".echo CAPTURE_BREAKPOINT_AUTO_CONTINUE; .time; kv; gh" bpe; gh' }
                    'detach' { 'qd' }
                    default { '' }
                }
                if ($controlCommands) {
                    $monitor.StandardInput.WriteLine($controlCommands)
                    $monitor.StandardInput.Flush()
                }
                Move-Item -LiteralPath $controlFile -Destination (Join-Path $WorkerOutput ('control-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.processed.txt'))
            }
            if (-not $attached -and (Test-Path -LiteralPath $cdbLog)) {
                $attached = [bool](Select-String -LiteralPath $cdbLog -Pattern '^CAPTURE_READY\s*$' -Quiet)
                if ($attached) {
                    Write-Json @{ state='monitoring'; worker_pid=$PID; game_pid=$target.Id; debugger_pid=$monitor.Id; session=$sessionDir } $statusFile
                }
            }
            if ((Get-Date) -ge $nextSnapshot) {
                Save-Logs (Join-Path $sessionDir 'latest')
                $nextSnapshot = (Get-Date).AddSeconds(5)
            }
            foreach ($dump in Get-ChildItem -LiteralPath $sessionDir -Filter '*.dmp') {
                if ($knownDumps.Add($dump.Name)) { Save-Logs (Join-Path $sessionDir ('at-' + $dump.BaseName)) }
            }
            if (-not $attached -and ((Get-Date)-$attachStarted).TotalSeconds -gt 30) {
                throw "30秒内调试器未就绪，将请求安全脱离。请查看 $cdbLog。"
            }
            Start-Sleep -Milliseconds 500
            $monitor.Refresh()
        }
        $monitor.WaitForExit()
        Save-Logs (Join-Path $sessionDir 'after') -Full
        $target.Refresh()
        $exitCode = if ($target.HasExited) { $target.ExitCode } else { $null }
        Write-Json @{ ended_at=[DateTimeOffset]::Now.ToString('o'); game_pid=$target.Id; game_exited=$target.HasExited;
            game_exit_code=$exitCode; debugger_exit_code=$monitor.ExitCode; attached=$attached;
            dumps=@(Get-ChildItem -LiteralPath $sessionDir -Filter '*.dmp' | Select-Object Name,Length);
            note='正常关闭也采集；不能把退出请求或关闭后的 UE4SS 异常直接判为游戏内崩溃。' } (Join-Path $sessionDir 'result.json')
        if (-not $target.HasExited) { throw '调试器已退出，游戏仍在运行；未自动重挂，请检查调试器日志。' }
        $waitUntil = (Get-Date).AddSeconds(60)
        Write-Json @{ state='waiting'; worker_pid=$PID; sessions=$sessionCount; until=$waitUntil.ToString('o') } $statusFile
    }
    Start-Sleep -Seconds 3
    $eventErrors = @()
    $events = @(Get-WinEvent -FilterHashtable @{ LogName='Application'; Id=1000,1001,1002; StartTime=$runStart } `
        -MaxEvents 200 -ErrorAction SilentlyContinue -ErrorVariable eventErrors | Where-Object { $_.Message -match 'Ace7Game|UE4SS' } |
        Select-Object TimeCreated,Id,ProviderName,Message)
    Write-Json @{ events=$events; errors=@($eventErrors | ForEach-Object { $_.ToString() }) } (Join-Path $WorkerOutput 'windows-events.json')
    Write-Json @{ state='finished'; sessions=$sessionCount; ended_at=[DateTimeOffset]::Now.ToString('o') } $statusFile
} catch {
    Write-Json @{ state='error'; error=$_.Exception.Message; sessions=$sessionCount; at=[DateTimeOffset]::Now.ToString('o') } $statusFile
    throw
} finally {
    # 仅发送安全脱离命令；不强杀带有软件断点的调试器或目标游戏。
    if ($monitor -and -not $monitor.HasExited) {
        try { $monitor.StandardInput.WriteLine('qd'); $monitor.StandardInput.Flush() } catch { }
    }
    if ($ownsMutex) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
