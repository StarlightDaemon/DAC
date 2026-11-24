#Requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer,[Parameter(Mandatory)][string]$Archive,
      [Parameter(Mandatory)][string]$Baseline,[switch]$Capture)
. (Join-Path $PSScriptRoot 'Safety.Common.ps1')
$findings = [Collections.Generic.List[object]]::new()
$private = [ordered]@{}
function Check($Id,[scriptblock]$Read) {
    try { & $Read; $findings.Add((New-SafetyFinding $Id PASS 'Read-only comparison passed.')) }
    catch { $findings.Add((New-SafetyFinding $Id FAIL (Get-SafetyFailure $_))) }
}
Check payload {
    $receipt = Assert-SafetyArtifact $Installer $Archive
    $private.state = Get-SafetyState
    $private.maintenance = @(Assert-SafetyInstalledPayload $receipt $private.state.target)
    # Record Inno files separately; their hashes are observations, not identities
    # derived from the pinned product ZIP. Setup generated them during install.
}
Check preservation {
    $baselinePath = Resolve-SafetyPath $Baseline
    if ((Get-Item -LiteralPath $baselinePath).Length -gt 4MB) { throw 'BASELINE_LIMIT' }
    $before = Get-Content -LiteralPath $baselinePath -Raw | ConvertFrom-Json -AsHashtable
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    try {
        if ($before.identity.sid -cne $identity.User.Value -or $before.identity.machine -cne $env:COMPUTERNAME -or
            $before.identity.session -ne (Get-Process -Id $PID).SessionId -or $before.identity.elevated -ne $false -or
            [Security.Principal.WindowsPrincipal]::new($identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'BASELINE_IDENTITY' }
    } finally { $identity.Dispose() }
    $now = Get-SafetyState
    if ($before.state.target -ine $now.target -or
        (ConvertTo-Json -InputObject @($before.state.settings | Sort-Object path) -Depth 8 -Compress) -cne (ConvertTo-Json -InputObject @($now.settings | Sort-Object path) -Depth 8 -Compress) -or
        (ConvertTo-Json -InputObject $before.state.startup -Compress) -cne (ConvertTo-Json -InputObject $now.startup -Compress)) { throw 'STATE_CHANGED' }
}
Check integration {
    $state = Get-SafetyState
    if ($state.uninstall.DisplayName -cne 'DAC' -or $state.uninstall.InstallLocation.TrimEnd('\') -ine $state.target.TrimEnd('\')) { throw 'UNINSTALL_REGISTRATION' }
    $command = [string]$state.uninstall.UninstallString
    if ($command -ine ('"'+(Join-Path $state.target 'unins000.exe')+'"')) { throw 'UNINSTALL_TARGET' }
    $link = Resolve-SafetyPath (Join-Path ([Environment]::GetFolderPath('Programs')) 'DAC/DAC.lnk')
    if (!(Test-Path -LiteralPath $link)) { throw 'SHORTCUT_MISSING' }
    $shell = New-Object -ComObject WScript.Shell
    try {
        $shortcut = $shell.CreateShortcut($link) # Read only: never call Save().
        try { if ($shortcut.TargetPath -ine (Join-Path $state.target 'DAC.exe') -or $shortcut.Arguments) { throw 'SHORTCUT_TARGET' } }
        finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut) }
    } finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell) }
    $private.uninstall=$state.uninstall
}
Check noCurrentController {
    if (@(Get-CimInstance Win32_Process -OperationTimeoutSec 10 -Filter "Name = 'DAC.exe'").Count) { throw 'CONTROLLER_PRESENT' }
}
$findings.Add((New-SafetyFinding observation BLOCKED 'A snapshot cannot prove Setup never elevated/launched DAC, or authenticate generated uninstaller files. Review attended Setup observations/log, sidecar and uninstaller evidence; do not execute them to test identity.'))
$findings.Add((New-SafetyFinding authority BLOCKED 'Unchanged preferences do not prove automation/hardware are disabled. Review existing settings privately before a separately authorized first launch.'))
$summary=Get-SafetySummary $findings
if ($Capture) { $summary.evidenceId=Save-SafetyEvidence $summary $private }
$summary | ConvertTo-Json -Depth 8
if ($summary.status -eq 'FAIL') { exit 3 }; exit 2
