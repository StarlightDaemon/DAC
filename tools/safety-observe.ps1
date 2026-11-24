#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][int]$ProcessId,[Parameter(Mandatory)][string]$Executable,
      [Parameter(Mandatory)][datetime]$StartUtc,[ValidateRange(1,30)][int]$Seconds=10,
      [ValidateRange(2,6)][int]$Stage=2,
      [ValidateSet('M1','M2','M3')][string[]]$Targets=@(),
      [ValidateSet('Unknown','Pass','Fail','Aborted')][string]$VisibleOutcome='Unknown',
      [ValidateSet('None','Tray','Hotkey','OwnedStop','TaskManager','External','WindowsRecovery')][string]$RecoveryUsed='None',
      [switch]$Capture)
. (Join-Path $PSScriptRoot 'Safety.Common.ps1')
# Passive observer only. An identity match never grants stop/kill authority.
$private=[ordered]@{}; $process=$null
try {
    $path=Resolve-SafetyPath $Executable
    if ((Get-SafetyDigest $path) -cne $script:SafetyIdentity.executable) { throw 'EXECUTABLE_IDENTITY' }
    $process=Get-Process -Id $ProcessId -ErrorAction Stop
    $null=$process.Handle # Retain OS process identity across PID reuse and exit.
    $current=Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" -OperationTimeoutSec 10
    $owner=Invoke-CimMethod -InputObject $current -MethodName GetOwnerSid -OperationTimeoutSec 10
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    try {
        if ($owner.ReturnValue -ne 0 -or $owner.Sid -cne $identity.User.Value -or
            $process.SessionId -ne (Get-Process -Id $PID).SessionId -or
            $current.ExecutablePath -ine $path -or $process.Path -ine $path -or
            $process.StartTime.ToUniversalTime().Ticks -ne $StartUtc.ToUniversalTime().Ticks -or $process.HasExited) { throw 'OWNERSHIP_AMBIGUOUS' }
    } finally { $identity.Dispose() }
    $samples=[Collections.Generic.List[object]]::new()
    for ($i=0;$i -lt $Seconds;$i++) {
        $process.Refresh()
        if ($process.HasExited) { break }
        $samples.Add([ordered]@{utc=[DateTime]::UtcNow.ToString('o');handles=$process.HandleCount;workingSet=$process.WorkingSet64;cpuSeconds=$process.TotalProcessorTime.TotalSeconds})
        if ($process.WaitForExit(1000)) { break }
    }
    $exited=$process.HasExited
    $private=[ordered]@{pid=$ProcessId;path=$path;startUtc=$StartUtc.ToUniversalTime().ToString('o');samples=@($samples.ToArray());exitObserved=$exited;exitUtc=$(if ($exited) {$process.ExitTime.ToUniversalTime().ToString('o')} else {$null})}
    $summary=[ordered]@{schema=1;source=$script:SafetyIdentity.source;executableSha256=$script:SafetyIdentity.executable;stage=$Stage;targets=$Targets;automated=[ordered]@{identityMatched=$true;exitObserved=$exited;samples=$samples.Count};operatorReported=[ordered]@{visibleOutcome=$VisibleOutcome;recoveryUsed=$RecoveryUsed};inputCounters='Use the existing DAC redacted diagnostic export; no input is intercepted.'}
    if ($Capture) { $summary.evidenceId=Save-SafetyEvidence $summary $private }
    $summary | ConvertTo-Json -Depth 8
} catch {
    [ordered]@{status='BLOCKED';reason='Exact executable, PID, start time, same-user/session ownership or observation unavailable. No process action was taken.'} | ConvertTo-Json
    exit 2
} finally { if ($process) {$process.Dispose()} }
