# 每次启动游戏前运行：保存最初异常、退出调用栈及独立日志。
[CmdletBinding()]
param([switch]$WaitOnly, [switch]$CheckOnly,
    [string]$GameRoot = 'E:\SteamLibrary\steamapps\common\ACE COMBAT 7',
    [string]$WorkerOutput = '')
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$pwshPath = (Get-Command pwsh -ErrorAction Stop).Source
function Write-Json($Value, [string]$Path) {
    $temporary = $Path + '.writing'
    $Value | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temporary -Encoding utf8
    [IO.File]::Move($temporary, $Path, $true)
}
function Write-CaptureWarning([string]$Message) {
    Write-Json @{ at=[DateTimeOffset]::Now.ToString('o'); message=$Message } (Join-Path $WorkerOutput 'warning.json')
    # 后台不能只把错误写进日志；短时提示，不终止游戏。
    try { $popup = New-Object -ComObject WScript.Shell; $null = $popup.Popup($Message, 8, 'AC7取证需要注意', 48) } catch { }
}
function Get-DumpInventory([string]$Directory) {
    @(Get-ChildItem -LiteralPath $Directory -Filter '*.dmp' | ForEach-Object {
        $stream = [IO.File]::OpenRead($_.FullName)
        try { $header = [byte[]]::new(4); $read = $stream.Read($header,0,4) } finally { $stream.Dispose() }
        @{ name=$_.Name; bytes=$_.Length; valid_header=($read -eq 4 -and [Text.Encoding]::ASCII.GetString($header) -eq 'MDMP'); sha256=(Get-FileHash -LiteralPath $_.FullName).Hash }
    })
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
function Start-Debugger([string[]]$Arguments, [string]$Directory) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $debugger
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = 'Hidden'
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $startInfo.ArgumentList.Add($argument.Trim('"')) }
    $child = [Diagnostics.Process]::Start($startInfo)
    # 两条输出管道持续排空并独立存档，不依赖启动窗口的控制台句柄。
    $script:debuggerStreams = @(
        [IO.File]::Open((Join-Path $Directory 'debugger.stdout.log'),'Create','Write','Read'),
        [IO.File]::Open((Join-Path $Directory 'debugger.stderr.log'),'Create','Write','Read'))
    $script:debuggerCopies = @(
        $child.StandardOutput.BaseStream.CopyToAsync($script:debuggerStreams[0]),
        $child.StandardError.BaseStream.CopyToAsync($script:debuggerStreams[1]))
    return $child
}
# 不下载符号，不吞异常；小转储有上限，致命异常/错误框/异常退出共用一份完整内存预算。
function Write-DebugCommands([string]$Directory) {
    $dumpPath = $Directory.Replace('\','/')
    if ($dumpPath -match '[\s";]') { throw '取证目录不能包含空格、引号或分号，请使用简单项目路径。' }
    $commands = [Collections.Generic.List[string]]::new()
    $commands.Add('.symopt+ 0x100')
    $commands.Add('r @$t0=0; r @$t1=0; r @$t2=0; r @$t3=0; r @$t5=0')
    $free = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($Directory)).AvailableFreeSpace
    $fullOption = if ($free -ge 8GB) { '/mA' } else { '/mipud' }
    Write-Json @{ full_memory_enabled=($fullOption -eq '/mA'); available_bytes=$free; full_dump_limit=1; small_dump_limits=@(8,2,2) } (Join-Path $Directory 'capture-policy.json')
    $full = '.if (@$t5 < 1) { r @$t5=@$t5+1; .dump ' + $fullOption + ' /u ' + $dumpPath + '/primary.dmp }'
    $second = '.echo CAPTURE_SECOND_CHANCE; .time; .exr -1; r; ~* kv; ' + $full + '; .if (@$t1 < 2) { r @$t1=@$t1+1; .dump /mipud /u ' + $dumpPath + '/unhandled.dmp }; gn'
    $commands.Add('sxd -c2 "' + $second + '" *')
    foreach ($code in @('c0000005','c0000409','c0000374','c00000fd','c000001d','40000015')) {
        $first = '.echo CAPTURE_FIRST_CHANCE; .time; .exr -1; r; kv; .if (@$t0 < 8) { r @$t0=@$t0+1; .dump /mipud /u ' + $dumpPath + '/first-chance.dmp }; gn'
        $commands.Add('sxe -c "' + $first + '" -c2 "' + $second + '" ' + $code)
    }
    foreach ($exitFunction in @('RtlExitUserProcess','NtTerminateProcess')) {
        $exitStatus = if ($exitFunction -eq 'NtTerminateProcess') { '@rdx' } else { '@rcx' }
        $onExit = '.echo CAPTURE_EXIT_REQUEST_' + $exitFunction + '; .time; r; ~* kv; .if (' + $exitStatus + ' != 0) { ' + $full + ' }; .if (@$t2 < 2) { r @$t2=@$t2+1; .dump /mipud /u ' + $dumpPath + '/exit-request.dmp }; gc'
        # 硬件断点不改目标指令，避免异常脱离后留下int3字节。
        $commands.Add('ba e 1 ntdll!' + $exitFunction + ' "' + $onExit + '"')
    }
    foreach ($kind in @('W','A')) {
        $show = if ($kind -eq 'W') { 'du' } else { 'da' }
        $message = '.echo CAPTURE_MESSAGEBOX_' + $kind + '; .time; ' + $show + ' @rdx L1000; ' + $show + ' @r8 L100; ~* kv; .if (@$t3 < 2) { r @$t3=@$t3+1; ' + $full + ' }; gc'
        $commands.Add('ba e 1 user32!MessageBox' + $kind + ' "' + $message + '"')
    }
    # F12 可触发系统调试热键，必须自动放行，不能把无人值守采集变成全进程暂停。
    $commands.Add('sxe -c ".echo CAPTURE_BREAKPOINT_AUTO_CONTINUE; .time; kv; gh" bpe')
    $commands.Add('sxn out')
    $commands.Add('bl')
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
    # 启动期间单独持锁，避免两个入口同时通过进程检查。
    $launchMutex = [Threading.Mutex]::new($false, 'Local\AC7MultiSPCrashCaptureLauncher')
    try { $launchOwned = $launchMutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $launchOwned = $true }
    if (-not $launchOwned) {
        $message = '另一个取证入口正在启动，请等待其完成后再操作。'
        try { $popup = New-Object -ComObject WScript.Shell; $null = $popup.Popup($message, 0, 'AC7取证无法启动', 48) } catch { }
        throw $message
    }
    # 不复用旧状态：工作进程或本工具调试器残留时，阻止启动并明确提示。
    $existing = @(Get-CimInstance Win32_Process | Where-Object {
        ($_.Name -match '^(pwsh|powershell)\.exe$' -and $_.ProcessId -ne $PID -and
            $_.CommandLine -like '*start_crash_capture.ps1*' -and $_.CommandLine -like '*-WorkerOutput*') -or
        ($_.Name -ieq 'cdb.exe' -and $_.CommandLine -like '*crash-capture-*' -and $_.CommandLine -like '*capture.cdb*')
    })
    if ($existing.Count) {
        $details = ($existing | ForEach-Object { '{0} PID={1}' -f $_.Name,$_.ProcessId }) -join '；'
        $message = "已有取证进程或调试器运行（$details）。本次未启动游戏；请先结束旧取证会话，再重新启动。"
        try { $popup = New-Object -ComObject WScript.Shell; $null = $popup.Popup($message, 0, 'AC7取证无法启动', 48) } catch { }
        throw $message
    }
    $captureDir = Join-Path $taskRoot ('Temp/crash-capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    [IO.Directory]::CreateDirectory($captureDir) | Out-Null
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-WorkerOutput',('"'+$captureDir+'"'),'-GameRoot',('"'+$GameRoot+'"'))
    $worker = Start-Process -FilePath $pwshPath -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $captureDir 'worker.stdout.log') `
        -RedirectStandardError (Join-Path $captureDir 'worker.stderr.log')
    Write-Json @{ capture_revision=2; script_sha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash; started_at=[DateTimeOffset]::Now.ToString('o'); worker_pid=$worker.Id; debugger=$debugger;
        debugger_sha256=(Get-FileHash -LiteralPath $debugger).Hash; game_root=$GameRoot; wait_only=[bool]$WaitOnly;
        note='附加就绪以每个 PID 的 CAPTURE_READY 和 status.json 为准，正常退出也保留现场。' } (Join-Path $captureDir 'launch.json')
    $statusFile = Join-Path $captureDir 'status.json'
    $ready = $false
    for ($attempt=0; $attempt -lt 60; $attempt++) {
        if (Test-Path -LiteralPath $statusFile) {
            try { $state = Get-Content -LiteralPath $statusFile -Raw | ConvertFrom-Json } catch { $state = $null }
            if ($state.state -in @('waiting','attaching','monitoring')) { $ready = $true; break }
            if ($state.state -in @('error','needs_attention')) { throw "取证未就绪：$($state.error)。请查看 $captureDir。" }
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
            if ($state.state -in @('error','finished','needs_attention')) { throw "取证未就绪：$($state.error)。请查看 $captureDir。" }
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
$needsAttention = $false
if (-not ('AC7CaptureHealth' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AC7CaptureHealth {
    [DllImport("kernel32.dll",SetLastError=true)]
    public static extern bool CheckRemoteDebuggerPresent(IntPtr process, out bool present);
}
'@
}
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
        $targetHandle = $target.Handle # 退出前持有句柄，避免退出后才打开导致ExitCode丢失。
        $alreadyDebugged = $false
        if (-not [AC7CaptureHealth]::CheckRemoteDebuggerPresent($targetHandle,[ref]$alreadyDebugged)) { throw '无法查询游戏的调试连接状态。' }
        if ($alreadyDebugged) { throw '游戏已有调试器，未重复附加。请先处理旧取证会话。' }
        $sessionCount++
        $sessionDir = Join-Path $WorkerOutput ('session-{0}-pid-{1}' -f $sessionCount,$target.Id)
        [IO.Directory]::CreateDirectory($sessionDir) | Out-Null
        Save-Logs (Join-Path $sessionDir 'before')
        $commandFile = Write-DebugCommands $sessionDir
        $cdbLog = Join-Path $sessionDir 'debugger.log'
        $arguments = @('-pd','-G','-sins','-y',('"'+$sessionDir+'"'),'-logo',('"'+$cdbLog+'"'),'-cf',('"'+$commandFile+'"'),'-p',$target.Id)
        Write-Json @{ state='attaching'; worker_pid=$PID; game_pid=$target.Id; session=$sessionDir } $statusFile
        $monitor = Start-Debugger $arguments $sessionDir
        $attachStarted = Get-Date
        $attached = $false
        $nextSnapshot = Get-Date
        $knownDumps = [Collections.Generic.HashSet[string]]::new()
        $channelWarning = $false
        while (-not $monitor.HasExited) {
            # 运行中的CDB不保证立即读取stdin命令，不把排队命令当成已完成快照。
            $controlFile = Join-Path $WorkerOutput 'control.txt'
            if (Test-Path -LiteralPath $controlFile) {
                $control = (Get-Content -LiteralPath $controlFile -Raw).Trim()
                Write-Json @{ command=$control; status='not_executed'; reason='运行中不经stdin假报操作成功；请保留现场并由人工调试器处理，或正常退出游戏收尾。' } (Join-Path $WorkerOutput 'control-result.json')
                Move-Item -LiteralPath $controlFile -Destination (Join-Path $WorkerOutput ('control-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.processed.txt'))
            }
            if (-not $attached -and (Test-Path -LiteralPath $cdbLog)) {
                $debugText = Get-Content -LiteralPath $cdbLog -Raw
                if ($debugText -match '(?im)^(Syntax error|Unable to resolve|Couldn.t resolve|Too many breakpoints|Couldn.t insert|Invalid breakpoint)') { throw '调试器断点配置失败，不能报告就绪。' }
                $attached = [bool]($debugText -match '(?m)^CAPTURE_READY\s*$')
                if ($attached) {
                    Write-Json @{ state='monitoring'; heartbeat_at=[DateTimeOffset]::Now.ToString('o'); worker_pid=$PID; game_pid=$target.Id; debugger_pid=$monitor.Id; session=$sessionDir } $statusFile
                }
            }
            if ((Get-Date) -ge $nextSnapshot) {
                Save-Logs (Join-Path $sessionDir 'latest')
                $present = $false
                $checked = [AC7CaptureHealth]::CheckRemoteDebuggerPresent($targetHandle,[ref]$present)
                $channelOk = $attached -and $checked -and $present
                if ($attached -and -not $channelOk -and -not $target.HasExited -and -not $channelWarning) {
                    $channelWarning = $needsAttention = $true
                    Write-CaptureWarning "游戏仍运行，但取证连接无法确认。请保留当前现场，采集目录：$sessionDir"
                }
                $heartbeat = @{ state=$(if ($channelWarning) { 'needs_attention' } elseif ($attached) { 'monitoring' } else { 'attaching' }); heartbeat_at=[DateTimeOffset]::Now.ToString('o'); worker_pid=$PID; game_pid=$target.Id; debugger_pid=$monitor.Id; debugger_present=$present; session=$sessionDir }
                Write-Json $heartbeat $statusFile
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
        foreach ($copy in $script:debuggerCopies) { $null = $copy.GetAwaiter().GetResult() }
        foreach ($stream in $script:debuggerStreams) { $stream.Dispose() }
        Save-Logs (Join-Path $sessionDir 'after') -Full
        $target.Refresh()
        $exitCode = if ($target.HasExited) { $target.ExitCode } else { $null }
        $dumpInventory = @(Get-DumpInventory $sessionDir)
        $debugText = if (Test-Path -LiteralPath $cdbLog) { Get-Content -LiteralPath $cdbLog -Raw } else { '' }
        $hasExit = $debugText -match '(?m)^CAPTURE_EXIT_REQUEST_(RtlExitUserProcess|NtTerminateProcess)\s*$'
        $hasFault = $debugText -match '(?m)^CAPTURE_(FIRST_CHANCE|SECOND_CHANCE|MESSAGEBOX_[WA])\s*$'
        $validDumps = @($dumpInventory | Where-Object { $_.valid_header -and $_.bytes -ge 32 })
        $writtenCount = [regex]::Matches($debugText,'(?m)^Dump successfully written\s*$').Count
        $complete = $attached -and -not $channelWarning -and $target.HasExited -and $hasExit -and $validDumps.Count -gt 0 -and $validDumps.Count -eq $dumpInventory.Count -and $writtenCount -ge $dumpInventory.Count
        $quality = if (-not $complete) { 'incomplete' } elseif ($exitCode -eq 0 -and -not $hasFault) { 'exit_captured_no_fault_observed' } elseif ($hasFault) { 'fault_context_captured' } else { 'exit_only_root_cause_unknown' }
        if (-not $complete -or ($exitCode -ne 0 -and -not $hasFault)) {
            $needsAttention = $true
            Write-CaptureWarning "游戏已退出，取证结果：$quality。不能据此判定没有错误。现场：$sessionDir"
        }
        Write-Json @{ ended_at=[DateTimeOffset]::Now.ToString('o'); game_pid=$target.Id; game_exited=$target.HasExited;
            game_exit_code=$exitCode; debugger_exit_code=$monitor.ExitCode; attached=$attached;
            capture_complete=$complete; capture_quality=$quality; debugger_confirmed_writes=$writtenCount; exit_hook_observed=$hasExit; fault_observed=$hasFault; dumps=$dumpInventory;
            note='正常关闭也采集；不能把退出请求或关闭后的 UE4SS 异常直接判为游戏内崩溃。' } (Join-Path $sessionDir 'result.json')
        if (-not $target.HasExited) { throw '调试器已退出，游戏仍在运行；未自动重挂，请检查调试器日志。' }
        $waitUntil = (Get-Date).AddSeconds(60)
        if ($needsAttention) { break }
        Write-Json @{ state='waiting'; heartbeat_at=[DateTimeOffset]::Now.ToString('o'); worker_pid=$PID; sessions=$sessionCount; until=$waitUntil.ToString('o') } $statusFile
    }
    Start-Sleep -Seconds 3
    $eventErrors = @()
    $events = @(Get-WinEvent -FilterHashtable @{ LogName='Application'; Id=1000,1001,1002; StartTime=$runStart } `
        -MaxEvents 200 -ErrorAction SilentlyContinue -ErrorVariable eventErrors | Where-Object { $_.Message -match 'Ace7Game|UE4SS' } |
        Select-Object TimeCreated,Id,ProviderName,Message)
    Write-Json @{ events=$events; errors=@($eventErrors | ForEach-Object { $_.ToString() }) } (Join-Path $WorkerOutput 'windows-events.json')
    Write-Json @{ state=$(if ($needsAttention) { 'needs_attention' } else { 'finished' }); error=$(if ($needsAttention) { '采集不完整或仅保留退出现场，请查看warning.json和session下result.json。' } else { '' }); sessions=$sessionCount; ended_at=[DateTimeOffset]::Now.ToString('o') } $statusFile
} catch {
    Write-Json @{ state='error'; error=$_.Exception.Message; sessions=$sessionCount; at=[DateTimeOffset]::Now.ToString('o') } $statusFile
    Write-CaptureWarning ('取证出错：' + $_.Exception.Message + '；目录：' + $WorkerOutput)
    throw
} finally {
    # 只请求脱离，不宣称运行中stdin请求已生效，不强杀调试器或游戏。
    if ($monitor -and -not $monitor.HasExited) {
        try { $monitor.StandardInput.WriteLine('qd'); $monitor.StandardInput.Flush() } catch { }
    }
    if ($ownsMutex) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
